#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# dependencies = [
#   "pyyaml",
# ]
# ///
"""Prepare missing assets, create configs, then run voxel carving."""

import argparse
import os
import re
import shutil
import subprocess
import tempfile
from datetime import datetime
from html import unescape
from pathlib import Path
from urllib.parse import unquote, urlparse
from urllib.request import Request, urlopen

from create_loader_config import (
    BatchObjectConfig,
    COLOR_METHODS,
    DEFAULT_RESOLUTION,
    DEFAULT_VOLUME_MAX,
    DEFAULT_VOLUME_MIN,
    object_names,
    write_batch_config,
)

ROOT = Path(__file__).resolve().parents[1]
OBJECTS_ROOT = Path("local/datasets")
DOWNLOADS = Path("local/downloads")
MASKS = Path("local/annotations/segmentation_masks")
CAMERAS = Path("local/annotations/camera")
RESULTS = Path("local/results")
UV = ["uv", "run", "--script", "--python", "3.14"]
UA = {"User-Agent": "CoVoCa dataset pipeline"}
DEFAULT_WORKERS = min(4, os.cpu_count() or 1)


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
        objects = (
            [source]
            if (source / "images").is_dir()
            else [path for path in source.iterdir() if (path / "images").is_dir()]
        )
        if not objects:
            raise ValueError("Archive did not contain object folders with images/")
        root.mkdir(parents=True, exist_ok=True)
        for object_dir in objects:
            shutil.copytree(object_dir, root / object_dir.name, dirs_exist_ok=True)
            print(f"Installed {object_dir.name} -> {root / object_dir.name}")
        return sorted(path.name for path in objects)


def selected_object_args(names: list[str]) -> list[object]:
    return [item for name in names for item in ("--object", name)]


def image_stems(object_dir: Path) -> set[str]:
    image_extensions = {".jpg", ".jpeg", ".png", ".webp"}
    return {
        path.stem
        for path in (object_dir / "images").iterdir()
        if path.is_file() and path.suffix.casefold() in image_extensions
    }


def has_masks(object_dir: Path) -> bool:
    masks = object_dir / "masks"
    return masks.is_dir() and all(
        (masks / f"{stem}.png").is_file() for stem in image_stems(object_dir)
    )


def has_camera(object_dir: Path) -> bool:
    camera = object_dir / "camera"
    return (camera / "intrinsics.yaml").is_file() and (camera / "poses.yaml").is_file()


def install_tree(source: Path, target: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"Generated asset directory does not exist: {source}")
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)


def prepare_download(
    args: argparse.Namespace, downloads_root: Path, objects_root: Path
) -> None:
    if args.dataset_url and args.archive:
        raise ValueError("Use either --dataset-url or --archive, not both")
    if args.dataset_url:
        unpack(download(args.dataset_url, downloads_root), objects_root)
    elif args.archive:
        unpack(full(args.archive), objects_root)


def generate_and_install_masks(
    args: argparse.Namespace, objects_root: Path, masks_root: Path, names: list[str]
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
            "--objects-root",
            objects_root,
            "--output-root",
            masks_root,
            "--device",
            device,
            "--sam-model",
            full(args.sam_model),
            *selected_object_args(names),
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
        install_tree(run_dir / name / "masks", objects_root / name / "masks")


def generate_and_install_camera(
    args: argparse.Namespace,
    objects_root: Path,
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
                "--objects-root",
                objects_root,
                "--output-root",
                cameras_root,
                *shared_intrinsics,
                *selected_object_args(names),
            ]
        )
    )
    for name in names:
        install_tree(run_dir / name / "camera", objects_root / name / "camera")


def prepare_assets(
    args: argparse.Namespace,
    objects_root: Path,
    masks_root: Path,
    cameras_root: Path,
    names: list[str],
) -> None:
    mask_targets = [
        name
        for name in names
        if args.regenerate_masks or not has_masks(objects_root / name)
    ]
    camera_targets = [
        name
        for name in names
        if args.regenerate_camera or not has_camera(objects_root / name)
    ]

    if mask_targets:
        print("Generating masks for: " + ", ".join(mask_targets), flush=True)
        generate_and_install_masks(args, objects_root, masks_root, mask_targets)
    else:
        print("Using existing masks from object folders", flush=True)

    if camera_targets:
        print("Generating camera YAML for: " + ", ".join(camera_targets), flush=True)
        generate_and_install_camera(args, objects_root, cameras_root, camera_targets)
    else:
        print("Using existing camera YAML from object folders", flush=True)

    missing = [
        f"{name}: masks/" for name in names if not has_masks(objects_root / name)
    ]
    missing += [
        f"{name}: camera/" for name in names if not has_camera(objects_root / name)
    ]
    if missing:
        raise FileNotFoundError(
            "Asset generation did not produce required files:\n  - "
            + "\n  - ".join(missing)
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
    objects_root: Path,
    results_run: Path,
    names: list[str],
) -> Path:
    batch_objects: list[BatchObjectConfig] = []

    for name in names:
        result = results_run / name
        result.mkdir(parents=True, exist_ok=True)
        shutil.copytree(
            objects_root / name / "masks", result / "masks", dirs_exist_ok=True
        )
        shutil.copytree(
            objects_root / name / "camera", result / "camera", dirs_exist_ok=True
        )

        batch_objects.append(
            BatchObjectConfig(
                name=name,
                volume_min=args.volume_min,
                volume_max=args.volume_max,
                resolution=args.resolution,
                color_methods=args.color_methods,
                no_color=args.no_color,
            )
        )

    batch_config = results_run / "voxel_carving_batch.yaml"
    # Run the C++ binary once; it distributes objects according to workers.
    write_batch_config(
        batch_config=batch_config,
        objects_root=objects_root,
        output_dir=results_run,
        workers=args.workers,
        objects=batch_objects,
        foreground_threshold=1,
    )
    run(
        [binary, batch_config.resolve()],
        cwd=ROOT,
        log=results_run / "voxel_carving.log",
    )

    for name in names:
        result = results_run / name
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
        "--object",
        action="append",
        help="process only this object; omit to process all objects",
    )
    parser.add_argument(
        "--objects-root",
        type=Path,
        default=OBJECTS_ROOT,
        help="root containing one subfolder per object",
    )
    parser.add_argument("--downloads-root", type=Path, default=DOWNLOADS)
    parser.add_argument("--results-root", type=Path, default=RESULTS)
    parser.add_argument("--masks-root", type=Path, default=MASKS)
    parser.add_argument("--camera-root", type=Path, default=CAMERAS)
    parser.add_argument(
        "--regenerate-masks",
        action="store_true",
        help="replace existing masks under the object folders",
    )
    parser.add_argument(
        "--regenerate-camera",
        action="store_true",
        help="replace existing camera YAML under the object folders",
    )
    parser.add_argument("--device", choices=("cuda", "cpu"), default="cuda")
    parser.add_argument(
        "--sam-model", type=Path, default=Path("local/models/sam2.1_l.pt")
    )
    parser.add_argument(
        "--prompt", action="append", help="mask prompt override as OBJECT=TEXT"
    )
    parser.add_argument(
        "--shared-intrinsics",
        type=Path,
        help="reuse a camera_intrinsics.yaml for camera generation",
    )
    parser.add_argument("--volume-min", nargs=3, type=float, default=DEFAULT_VOLUME_MIN)
    parser.add_argument("--volume-max", nargs=3, type=float, default=DEFAULT_VOLUME_MAX)
    parser.add_argument("--resolution", nargs=3, type=int, default=DEFAULT_RESOLUTION)
    parser.add_argument(
        "--color-methods", nargs="+", choices=COLOR_METHODS, default=["average"]
    )
    parser.add_argument("--no-color", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument(
        "--workers",
        type=int,
        default=DEFAULT_WORKERS,
        help="number of objects to carve in parallel",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.workers < 1:
        raise ValueError("--workers must be at least 1")
    objects_root, downloads_root, results_root, build_dir = map(
        full,
        (args.objects_root, args.downloads_root, args.results_root, args.build_dir),
    )
    masks_root, cameras_root = map(full, (args.masks_root, args.camera_root))

    prepare_download(args, downloads_root, objects_root)
    names = object_names(objects_root, args.object)
    prepare_assets(args, objects_root, masks_root, cameras_root, names)
    results_run = results_root / datetime.now().strftime("%Y%m%d_%H%M%S")
    binary = build_binary(args, build_dir)
    results_run = run_reconstructions(args, binary, objects_root, results_run, names)

    print(f"Pipeline output: {results_run}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
