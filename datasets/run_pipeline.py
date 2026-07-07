#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# ///
"""Prepare missing assets, create configs, then run voxel carving."""

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

from create_loader_config import write_configs

ROOT = Path(__file__).resolve().parents[1]
DATASETS = Path("local/datasets")
DOWNLOADS = Path("local/downloads")
MASKS = Path("local/annotations/segmentation_masks")
CAMERAS = Path("local/annotations/camera")
CONFIGS = Path("local/configs")
RESULTS = Path("local/results")
UV = ["uv", "run", "--script", "--python", "3.14"]
UA = {"User-Agent": "CoVoCa dataset pipeline"}
COLOR_METHODS = ("average", "best_view", "weighted_average", "median")


def full(path: Path) -> Path:
    return path if path.is_absolute() else ROOT / path


def run(cmd: list[object], cwd: Path = ROOT, log: Path | None = None) -> list[str]:
    print("+ " + " ".join(map(str, cmd)), flush=True)
    lines: list[str] = []
    with (
        log.open("w", encoding="utf-8") if log else tempfile.TemporaryFile("w")
    ) as stream:
        process = subprocess.Popen(
            [str(part) for part in cmd],
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
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
    found = [
        line.removeprefix("Output: ").strip()
        for line in lines
        if line.startswith("Output: ")
    ]
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
        name = (
            unquote(match.group(1)) if match else Path(unquote(urlparse(url).path)).name
        )
        target = root / (Path(name).name or "datasets.zip")
        with target.open("wb") as stream:
            shutil.copyfileobj(response, stream)
    print(f"Downloaded {target}")
    return target


def unpack(archive: Path, root: Path) -> list[str]:
    with tempfile.TemporaryDirectory(prefix="extract_", dir=archive.parent) as tmp_name:
        tmp = Path(tmp_name)
        shutil.unpack_archive(archive, tmp)
        source = next(
            (
                path
                for path in (tmp / "local" / "datasets", tmp / "datasets", tmp)
                if path.is_dir()
            ),
            tmp,
        )
        datasets = (
            [source]
            if (source / "images").is_dir()
            else [path for path in source.iterdir() if (path / "images").is_dir()]
        )
        if not datasets:
            raise ValueError("Archive did not contain dataset folders with images/")
        root.mkdir(parents=True, exist_ok=True)
        for dataset in datasets:
            shutil.copytree(dataset, root / dataset.name, dirs_exist_ok=True)
            print(f"Installed {dataset.name} -> {root / dataset.name}")
        return sorted(path.name for path in datasets)


def datasets(root: Path, selected: list[str] | None) -> list[str]:
    keep = set(selected or [])
    names = sorted(
        path.name
        for path in root.iterdir()
        if path.is_dir() and (path / "images").is_dir()
    )
    names = [name for name in names if not keep or name in keep]
    if not names:
        raise FileNotFoundError(f"No datasets with images/ found under {root}")
    return names


def selected_dataset_args(names: list[str]) -> list[object]:
    return [item for name in names for item in ("--dataset", name)]


def image_stems(dataset: Path) -> set[str]:
    image_extensions = {".jpg", ".jpeg", ".png", ".webp"}
    return {
        path.stem
        for path in (dataset / "images").iterdir()
        if path.is_file() and path.suffix.casefold() in image_extensions
    }


def has_masks(dataset: Path) -> bool:
    masks = dataset / "masks"
    return masks.is_dir() and all(
        (masks / f"{stem}.png").is_file() for stem in image_stems(dataset)
    )


def has_camera(dataset: Path) -> bool:
    camera = dataset / "camera"
    return (camera / "intrinsics.yaml").is_file() and (camera / "poses.yaml").is_file()


def install_tree(source: Path, target: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Generated asset directory does not exist: {source}")
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)


def prepare_download(
    args: argparse.Namespace, downloads_root: Path, datasets_root: Path
) -> None:
    if args.dataset_url and args.archive:
        raise ValueError("Use either --dataset-url or --archive, not both")
    if args.dataset_url:
        unpack(download(args.dataset_url, downloads_root), datasets_root)
    elif args.archive:
        unpack(full(args.archive), datasets_root)


def generate_and_install_masks(
    args: argparse.Namespace, datasets_root: Path, masks_root: Path, names: list[str]
) -> None:
    if not names:
        return

    prompt_args = [
        item for prompt in args.prompt or [] for item in ("--prompt", prompt)
    ]

    def mask_cmd(device: str) -> list[object]:
        return [
            *UV,
            "datasets/generate_masks.py",
            "--datasets-root",
            datasets_root,
            "--output-root",
            masks_root,
            "--device",
            device,
            "--sam-model",
            full(args.sam_model),
            *selected_dataset_args(names),
            *prompt_args,
        ]

    try:
        run_dir = output(run(mask_cmd(args.device)))
    except subprocess.CalledProcessError:
        if args.device != "cuda":
            raise
        print("CUDA mask generation failed; retrying on CPU", flush=True)
        run_dir = output(run(mask_cmd("cpu")))

    for name in names:
        install_tree(run_dir / name / "masks", datasets_root / name / "masks")


def generate_and_install_camera(
    args: argparse.Namespace,
    datasets_root: Path,
    cameras_root: Path,
    names: list[str],
) -> None:
    if not names:
        return

    shared_intrinsics = (
        ["--shared-intrinsics", full(args.shared_intrinsics)]
        if args.shared_intrinsics
        else []
    )
    run_dir = output(
        run(
            [
                *UV,
                "datasets/generate_camera.py",
                "--datasets-root",
                datasets_root,
                "--output-root",
                cameras_root,
                *shared_intrinsics,
                *selected_dataset_args(names),
            ]
        )
    )
    for name in names:
        install_tree(run_dir / name / "camera", datasets_root / name / "camera")


def prepare_assets(
    args: argparse.Namespace,
    datasets_root: Path,
    masks_root: Path,
    cameras_root: Path,
    names: list[str],
) -> None:
    mask_targets = [
        name
        for name in names
        if args.regenerate_masks or not has_masks(datasets_root / name)
    ]
    camera_targets = [
        name
        for name in names
        if args.regenerate_camera or not has_camera(datasets_root / name)
    ]

    if mask_targets:
        print("Generating masks for: " + ", ".join(mask_targets), flush=True)
        generate_and_install_masks(args, datasets_root, masks_root, mask_targets)
    else:
        print("Using existing masks from local/datasets", flush=True)

    if camera_targets:
        print("Generating camera YAML for: " + ", ".join(camera_targets), flush=True)
        generate_and_install_camera(args, datasets_root, cameras_root, camera_targets)
    else:
        print("Using existing camera YAML from local/datasets", flush=True)

    missing = [
        f"{name}: masks/" for name in names if not has_masks(datasets_root / name)
    ]
    missing += [
        f"{name}: camera/" for name in names if not has_camera(datasets_root / name)
    ]
    if missing:
        raise FileNotFoundError(
            "Asset generation did not produce required files:\n  - "
            + "\n  - ".join(missing)
        )


def create_loader_configs(
    args: argparse.Namespace, datasets_root: Path, configs_root: Path, names: list[str]
) -> None:
    for name in names:
        dataset_config = configs_root / f"{name}.dataset.yaml"
        voxel_config = configs_root / f"{name}.voxel_carving.yaml"
        write_configs(
            dataset=name,
            datasets_root=datasets_root,
            masks_root=datasets_root,
            camera_root=datasets_root,
            dataset_config=dataset_config,
            voxel_config=voxel_config,
            foreground_threshold=1,
            volume_min=args.volume_min,
            volume_max=args.volume_max,
            resolution=args.resolution,
            color_methods=args.color_methods,
            no_color=args.no_color,
        )
        print(f"Wrote {dataset_config}")
        print(f"Wrote {voxel_config}")


def run_voxel_carving(
    binary: Path, configs_root: Path, result: Path, name: str
) -> None:
    dataset_config = configs_root / f"{name}.dataset.yaml"
    voxel_config = configs_root / f"{name}.voxel_carving.yaml"
    shutil.copy2(dataset_config, result / "dataset.yaml")
    shutil.copy2(voxel_config, result / "voxel_carving.yaml")
    run(
        [binary, dataset_config.resolve(), voxel_config.resolve()],
        cwd=result,
        log=result / "voxel_carving.log",
    )


def vertices(path: Path) -> int:
    with path.open(encoding="utf-8") as stream:
        return next(
            int(line.split()[-1])
            for line in stream
            if line.startswith("element vertex ")
        )


def voxel_grid_outputs(args: argparse.Namespace, result: Path) -> list[Path]:
    if args.no_color or len(args.color_methods) == 1:
        return [result / "voxel_grid.ply"]
    return [result / method / "voxel_grid.ply" for method in args.color_methods]


def build_binary(args: argparse.Namespace, build_dir: Path) -> Path:
    if not args.skip_build:
        run(["cmake", "-S", ".", "-B", build_dir])
        run(["cmake", "--build", build_dir, "-j"])

    binary = (build_dir / "main").resolve()
    if not binary.is_file():
        raise FileNotFoundError(f"Voxel carving binary not found: {binary}")
    return binary


def run_reconstructions(
    args: argparse.Namespace,
    binary: Path,
    datasets_root: Path,
    configs_root: Path,
    results_root: Path,
    names: list[str],
) -> Path:
    results_run = results_root / datetime.now().strftime("%Y%m%d_%H%M%S")
    for name in names:
        result = results_run / name
        result.mkdir(parents=True, exist_ok=True)
        shutil.copytree(
            datasets_root / name / "masks", result / "masks", dirs_exist_ok=True
        )
        shutil.copytree(
            datasets_root / name / "camera", result / "camera", dirs_exist_ok=True
        )
        run_voxel_carving(binary, configs_root, result, name)
        for voxel_grid in voxel_grid_outputs(args, result):
            if vertices(voxel_grid) == 0:
                raise RuntimeError(
                    f"{name}: empty voxel model; adjust --volume-min/--volume-max"
                )
    return results_run


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--dataset-url", help="download and extract this SyncAndShare URL first"
    )
    parser.add_argument(
        "--archive",
        type=Path,
        help="extract this already downloaded zip/tar archive first",
    )
    parser.add_argument(
        "--dataset",
        action="append",
        help="process only this dataset; omit to process all datasets",
    )
    parser.add_argument("--datasets-root", type=Path, default=DATASETS)
    parser.add_argument("--downloads-root", type=Path, default=DOWNLOADS)
    parser.add_argument("--results-root", type=Path, default=RESULTS)
    parser.add_argument("--masks-root", type=Path, default=MASKS)
    parser.add_argument("--camera-root", type=Path, default=CAMERAS)
    parser.add_argument(
        "--regenerate-masks",
        action="store_true",
        help="replace existing local/datasets masks",
    )
    parser.add_argument(
        "--regenerate-camera",
        action="store_true",
        help="replace existing local/datasets camera YAML",
    )
    parser.add_argument("--device", choices=("cuda", "cpu"), default="cuda")
    parser.add_argument(
        "--sam-model", type=Path, default=Path("local/models/sam2.1_l.pt")
    )
    parser.add_argument(
        "--prompt", action="append", help="mask prompt override as DATASET=TEXT"
    )
    parser.add_argument(
        "--shared-intrinsics",
        type=Path,
        help="reuse a camera_intrinsics.yaml for camera generation",
    )
    parser.add_argument(
        "--volume-min", nargs=3, type=float, default=[-0.05, 0.0, -0.05]
    )
    parser.add_argument("--volume-max", nargs=3, type=float, default=[0.05, 0.10, 0.05])
    parser.add_argument("--resolution", nargs=3, type=int, default=[100, 100, 100])
    parser.add_argument(
        "--color-methods", nargs="+", choices=COLOR_METHODS, default=["average"]
    )
    parser.add_argument("--no-color", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    datasets_root, downloads_root, results_root, build_dir = map(
        full,
        (args.datasets_root, args.downloads_root, args.results_root, args.build_dir),
    )
    masks_root, cameras_root = map(full, (args.masks_root, args.camera_root))
    configs_root = full(CONFIGS)

    prepare_download(args, downloads_root, datasets_root)
    names = datasets(datasets_root, args.dataset)
    prepare_assets(args, datasets_root, masks_root, cameras_root, names)
    create_loader_configs(args, datasets_root, configs_root, names)
    binary = build_binary(args, build_dir)
    results_run = run_reconstructions(
        args, binary, datasets_root, configs_root, results_root, names
    )

    print(f"Pipeline output: {results_run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
