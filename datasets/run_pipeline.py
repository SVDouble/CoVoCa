#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# ///
"""Download data, generate masks/cameras/configs, then run voxel carving."""

import argparse
import re
import shutil
import subprocess
import tempfile
from datetime import datetime
from html import unescape
from pathlib import Path
from urllib.parse import unquote, urlparse
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
DATASETS = Path("local/datasets")
DOWNLOADS = Path("local/downloads")
MASKS = Path("local/annotations/segmentation_masks")
CAMERAS = Path("local/annotations/camera")
CONFIGS = Path("local/configs")
RESULTS = Path("local/results")
UV = ["uv", "run", "--script", "--python", "3.14"]
UA = {"User-Agent": "CoVoCa dataset pipeline"}


def full(path: Path) -> Path:
    return path if path.is_absolute() else ROOT / path


def run(cmd: list[object], cwd: Path = ROOT, log: Path | None = None) -> list[str]:
    print("+ " + " ".join(map(str, cmd)), flush=True)
    lines: list[str] = []
    with (log.open("w", encoding="utf-8") if log else tempfile.TemporaryFile("w")) as stream:
        process = subprocess.Popen(
            [str(part) for part in cmd], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
        )
        assert process.stdout
        for line in process.stdout:
            print(line, end="", flush=True)
            stream.write(line)
            lines.append(line.rstrip())
        if process.wait():
            raise subprocess.CalledProcessError(process.returncode, cmd)
    return lines


def output(lines: list[str]) -> Path:
    found = [line.removeprefix("Output: ").strip() for line in lines if line.startswith("Output: ")]
    if not found:
        raise RuntimeError("Command did not print an Output line")
    return Path(found[-1])


def download(url: str, root: Path) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    with urlopen(Request(url, headers=UA)) as response:
        if "text/html" in response.headers.get("Content-Type", ""):
            page = response.read().decode("utf-8", "replace")
            match = re.search(r'["\'](https?://[^"\']+/dl/[^"\']+)["\']', page)
            if not match:
                raise ValueError(f"Could not find a download link on {url}")
            return download(unescape(match.group(1)), root)

        disposition = response.headers.get("Content-Disposition", "")
        match = re.search(r"filename\*?=(?:UTF-8''|\"?)([^\";]+)", disposition)
        name = unquote(match.group(1)) if match else Path(unquote(urlparse(url).path)).name
        target = root / (Path(name).name or "datasets.zip")
        with target.open("wb") as stream:
            shutil.copyfileobj(response, stream)
    print(f"Downloaded {target}")
    return target


def unpack(archive: Path, root: Path) -> list[str]:
    with tempfile.TemporaryDirectory(prefix="extract_", dir=archive.parent) as tmp_name:
        tmp = Path(tmp_name)
        shutil.unpack_archive(archive, tmp)
        source = next((path for path in (tmp / "local" / "datasets", tmp / "datasets", tmp) if path.is_dir()), tmp)
        datasets = [source] if (source / "images").is_dir() else [path for path in source.iterdir() if (path / "images").is_dir()]
        if not datasets:
            raise ValueError("Archive did not contain dataset folders with images/")
        root.mkdir(parents=True, exist_ok=True)
        for dataset in datasets:
            shutil.copytree(dataset, root / dataset.name, dirs_exist_ok=True)
            print(f"Installed {dataset.name} -> {root / dataset.name}")
        return sorted(path.name for path in datasets)


def datasets(root: Path, selected: list[str] | None) -> list[str]:
    keep = set(selected or [])
    names = sorted(path.name for path in root.iterdir() if path.is_dir() and (path / "images").is_dir())
    names = [name for name in names if not keep or name in keep]
    if not names:
        raise FileNotFoundError(f"No datasets with images/ found under {root}")
    return names


def vertices(path: Path) -> int:
    with path.open(encoding="utf-8") as stream:
        return next(int(line.split()[-1]) for line in stream if line.startswith("element vertex "))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--dataset-url", help="SyncAndShare direct download URL")
    parser.add_argument("--archive", type=Path, help="use an already downloaded zip/tar archive")
    parser.add_argument("--skip-download", action="store_true", help="use existing local/datasets")
    parser.add_argument("--dataset", action="append", help="process only this dataset; omit to process all datasets")
    parser.add_argument("--datasets-root", type=Path, default=DATASETS)
    parser.add_argument("--downloads-root", type=Path, default=DOWNLOADS)
    parser.add_argument("--results-root", type=Path, default=RESULTS)
    parser.add_argument("--device", choices=("cuda", "cpu"), default="cuda")
    parser.add_argument("--sam-model", type=Path, default=Path("local/models/sam2.1_l.pt"))
    parser.add_argument("--prompt", action="append", help="mask prompt override as DATASET=TEXT")
    parser.add_argument("--volume-min", nargs=3, type=float, default=[-0.05, 0.0, -0.05])
    parser.add_argument("--volume-max", nargs=3, type=float, default=[0.05, 0.10, 0.05])
    parser.add_argument("--resolution", nargs=3, type=int, default=[100, 100, 100])
    parser.add_argument("--color-method", default="average")
    parser.add_argument("--no-color", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    datasets_root, downloads_root, results_root, build_dir = map(
        full, (args.datasets_root, args.downloads_root, args.results_root, args.build_dir)
    )
    masks_root, cameras_root, configs_root = map(full, (MASKS, CAMERAS, CONFIGS))

    if not args.skip_download:
        try:
            url = args.dataset_url or input("SyncAndShare dataset URL (empty to use local/datasets): ").strip()
        except EOFError:
            url = args.dataset_url or ""
        archive = download(url, downloads_root) if url else full(args.archive) if args.archive else None
        if archive:
            unpack(archive, datasets_root)

    names = datasets(datasets_root, args.dataset)
    dataset_args = [item for name in names for item in ("--dataset", name)]
    prompt_args = [item for prompt in args.prompt or [] for item in ("--prompt", prompt)]

    def mask_cmd(device: str) -> list[object]:
        return [
            *UV, "datasets/generate_masks.py", "--datasets-root", datasets_root, "--output-root", masks_root,
            "--device", device, "--sam-model", full(args.sam_model), *dataset_args, *prompt_args,
        ]

    try:
        mask_run = output(run(mask_cmd(args.device)))
    except subprocess.CalledProcessError:
        if args.device != "cuda":
            raise
        print("CUDA mask generation failed; retrying on CPU", flush=True)
        mask_run = output(run(mask_cmd("cpu")))
    camera_run = output(run([
        *UV, "datasets/generate_camera.py", "--datasets-root", datasets_root, "--output-root", cameras_root, *dataset_args,
    ]))

    for name in names:
        run([
            *UV, "datasets/create_loader_config.py", "--dataset", name, "--datasets-root", datasets_root,
            "--masks-root", masks_root, "--camera-root", cameras_root, "--mask-run", mask_run.name,
            "--camera-run", camera_run.name, "--volume-min", *map(str, args.volume_min),
            "--volume-max", *map(str, args.volume_max), "--resolution", *map(str, args.resolution),
            "--color-method", args.color_method, *(["--no-color"] if args.no_color else []),
        ])

    if not args.skip_build:
        run(["cmake", "-S", ".", "-B", build_dir])
        run(["cmake", "--build", build_dir, "-j"])

    binary = (build_dir / "main").resolve()
    results_run = results_root / datetime.now().strftime("%Y%m%d_%H%M%S")
    for name in names:
        result = results_run / name
        result.mkdir(parents=True, exist_ok=True)
        shutil.copy2(configs_root / f"{name}.dataset.yaml", result / "dataset.yaml")
        shutil.copy2(configs_root / f"{name}.voxel_carving.yaml", result / "voxel_carving.yaml")
        shutil.copytree(mask_run / name / "masks", result / "masks", dirs_exist_ok=True)
        shutil.copytree(camera_run / name / "camera", result / "camera", dirs_exist_ok=True)
        run([binary, (configs_root / f"{name}.dataset.yaml").resolve(), (configs_root / f"{name}.voxel_carving.yaml").resolve()],
            cwd=result, log=result / "voxel_carving.log")
        if vertices(result / "voxel_grid.ply") == 0:
            raise RuntimeError(f"{name}: empty voxel model; adjust --volume-min/--volume-max")

    print(f"Pipeline output: {results_run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
