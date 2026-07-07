#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# dependencies = [
#   "pyyaml",
# ]
# ///
"""Create voxel-carving batch YAML configs for the C++ loader."""

import argparse
import os
from dataclasses import dataclass
from pathlib import Path

import yaml

OBJECTS_ROOT = Path("local/datasets")
CONFIGS = Path("local/configs")
RESULTS = Path("local/results/manual")
COLOR_METHODS = ("average", "best_view", "weighted_average", "median")
DEFAULT_VOLUME_MIN = [-0.02, -0.22, 0.0]
DEFAULT_VOLUME_MAX = [0.2, 0.06, 0.22]
DEFAULT_RESOLUTION = [120, 150, 120]


@dataclass(frozen=True)
class BatchObjectConfig:
    name: str
    volume_min: list[float]
    volume_max: list[float]
    resolution: list[int]
    color_methods: list[str]
    no_color: bool


def config_path(path: Path, config_file: Path) -> str:
    absolute = Path(os.path.abspath(path))
    base = Path(os.path.abspath(config_file.parent))
    return Path(os.path.relpath(absolute, base)).as_posix()


def object_paths(objects_root: Path, object_name: str) -> tuple[Path, Path, Path]:
    object_dir = objects_root / object_name
    return object_dir / "images", object_dir / "masks", object_dir / "camera"


def validate_object_data(objects_root: Path, object_name: str) -> None:
    images, masks, camera = object_paths(objects_root, object_name)
    checks = (
        (images, "images directory", images.is_dir),
        (masks, "masks directory", masks.is_dir),
        (camera, "camera directory", camera.is_dir),
        (
            camera / "intrinsics.yaml",
            "camera intrinsics file",
            (camera / "intrinsics.yaml").is_file,
        ),
        (camera / "poses.yaml", "camera poses file", (camera / "poses.yaml").is_file),
    )
    for path, label, ok in checks:
        if not ok():
            raise FileNotFoundError(f"Missing {label}: {path}")


def object_names(objects_root: Path, selected: list[str] | None) -> list[str]:
    if selected:
        return sorted(selected)

    names = sorted(
        path.name for path in objects_root.iterdir() if (path / "images").is_dir()
    )
    if not names:
        raise FileNotFoundError(
            f"No object folders with images/ found under {objects_root}"
        )
    return names


def read_batch_grids(
    path: Path,
) -> dict[str, tuple[list[float], list[float], list[int]]]:
    if not path.is_file():
        raise FileNotFoundError(f"Missing batch config: {path}")

    document = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    grids = {}
    for entry in document.get("objects", []):
        name = entry.get("name")
        grid = entry.get("voxel_grid", {})
        if not name or not {"min", "max", "resolution"} <= grid.keys():
            raise ValueError(f"Invalid object voxel_grid in {path}")
        grids[name] = (grid["min"], grid["max"], grid["resolution"])

    if not grids:
        raise ValueError(f"Could not read object voxel grids from {path}")
    return grids


def write_batch_config(
    *,
    batch_config: Path,
    objects_root: Path,
    output_dir: Path,
    workers: int,
    objects: list[BatchObjectConfig],
    foreground_threshold: int,
    validate_inputs: bool = True,
) -> None:
    if workers < 1:
        raise ValueError("--workers must be at least 1")
    if not objects:
        raise ValueError("Batch config needs at least one object")

    batch_config.parent.mkdir(parents=True, exist_ok=True)
    document = {
        "schema": "covoca.branch1.voxel_carving_batch.v1",
        "workers": workers,
        "output_dir": config_path(output_dir, batch_config),
        "objects": [],
    }

    for entry in objects:
        if validate_inputs:
            validate_object_data(objects_root, entry.name)
        images, masks, camera = object_paths(objects_root, entry.name)
        object_entry = {
            "name": entry.name,
            "paths": {
                "images_dir": config_path(images, batch_config),
                "masks_dir": config_path(masks, batch_config),
                "camera_dir": config_path(camera, batch_config),
            },
            "foreground_threshold": foreground_threshold,
            "voxel_grid": {
                "min": entry.volume_min,
                "max": entry.volume_max,
                "resolution": entry.resolution,
            },
        }
        if not entry.no_color:
            object_entry["color"] = {"methods": entry.color_methods}
        document["objects"].append(object_entry)

    batch_config.write_text(yaml.safe_dump(document, sort_keys=False), encoding="utf-8")


def batch_objects(
    args: argparse.Namespace, names: list[str]
) -> list[BatchObjectConfig]:
    reference_grids = (
        read_batch_grids(args.reference_results / "voxel_carving_batch.yaml")
        if args.reference_results
        else {}
    )

    objects = []
    for name in names:
        if reference_grids:
            if name not in reference_grids:
                raise KeyError(f"{name} is not listed in {args.reference_results}")
            volume_min, volume_max, resolution = reference_grids[name]
        else:
            volume_min, volume_max, resolution = (
                args.volume_min,
                args.volume_max,
                args.resolution,
            )
        objects.append(
            BatchObjectConfig(
                name=name,
                volume_min=volume_min,
                volume_max=volume_max,
                resolution=resolution,
                color_methods=args.color_methods,
                no_color=args.no_color,
            )
        )
    return objects


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--object",
        action="append",
        help="object folder name; omit in batch mode to use all object folders",
    )
    parser.add_argument(
        "--objects-root",
        type=Path,
        default=OBJECTS_ROOT,
        help="root containing one subfolder per object",
    )
    parser.add_argument(
        "--batch-config",
        type=Path,
        help="output batch config",
    )
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--foreground-threshold", type=int, default=1)
    parser.add_argument(
        "--reference-results",
        type=Path,
        help="read per-object voxel_grid values from this previous results folder",
    )
    parser.add_argument("--volume-min", nargs=3, type=float, default=DEFAULT_VOLUME_MIN)
    parser.add_argument("--volume-max", nargs=3, type=float, default=DEFAULT_VOLUME_MAX)
    parser.add_argument("--resolution", nargs=3, type=int, default=DEFAULT_RESOLUTION)
    parser.add_argument(
        "--color-methods", nargs="+", choices=COLOR_METHODS, default=["average"]
    )
    parser.add_argument("--no-color", action="store_true")
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument(
        "--allow-missing-data",
        action="store_true",
        help="write configs even if local object data has not been downloaded yet",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    validate_inputs = not args.allow_missing_data
    names = (
        sorted(read_batch_grids(args.reference_results / "voxel_carving_batch.yaml"))
        if args.reference_results and not args.object
        else object_names(args.objects_root, args.object)
    )
    objects = batch_objects(args, names)

    batch_config = args.batch_config or CONFIGS / (
        f"{names[0]}.voxel_carving_batch.yaml"
        if len(names) == 1
        else "all_objects.voxel_carving_batch.yaml"
    )
    output_dir = args.output_dir or (
        RESULTS if len(names) == 1 else RESULTS / "all_objects"
    )
    write_batch_config(
        batch_config=batch_config,
        objects_root=args.objects_root,
        output_dir=output_dir,
        workers=args.workers,
        objects=objects,
        foreground_threshold=args.foreground_threshold,
        validate_inputs=validate_inputs,
    )
    print(f"Wrote {batch_config}")
    print(f"Output: {output_dir}")
    print(f"Run: ./build/main {batch_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
