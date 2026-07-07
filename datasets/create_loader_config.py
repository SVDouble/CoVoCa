#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# ///
"""Create object and voxel-carving YAML configs for the C++ loader."""

import argparse
from dataclasses import dataclass
from pathlib import Path

OBJECTS_ROOT = Path("local/datasets")
CONFIGS = Path("local/configs")
RESULTS = Path("local/results/manual")
COLOR_METHODS = ("average", "best_view", "weighted_average", "median")


@dataclass(frozen=True)
class BatchObjectConfig:
    name: str
    object_config: Path
    volume_min: list[float]
    volume_max: list[float]
    resolution: list[int]
    color_methods: list[str]
    no_color: bool


def yaml_list(values: list[float] | list[int] | list[str]) -> str:
    return "[" + ", ".join(map(str, values)) + "]"


def write_configs(
    *,
    object_name: str,
    objects_root: Path,
    object_config: Path,
    voxel_config: Path,
    output_dir: Path,
    foreground_threshold: int,
    volume_min: list[float],
    volume_max: list[float],
    resolution: list[int],
    color_methods: list[str],
    no_color: bool,
) -> None:
    object_config.parent.mkdir(parents=True, exist_ok=True)
    voxel_config.parent.mkdir(parents=True, exist_ok=True)

    object_dir = objects_root / object_name
    images = object_dir / "images"
    masks = object_dir / "masks"
    camera = object_dir / "camera"

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

    object_config.write_text(
        f"""schema: covoca.branch1.object.v1
name: {object_name}

paths:
  images_dir: {images.resolve().as_posix()}
  masks_dir: {masks.resolve().as_posix()}
  camera_dir: {camera.resolve().as_posix()}

foreground_threshold: {foreground_threshold}
""",
        encoding="utf-8",
    )
    voxel_lines = [
        "schema: covoca.branch1.voxel_carving.v1",
        f"name: {object_name}",
        f"output_dir: {output_dir.resolve().as_posix()}",
        "",
        "voxel_grid:",
        f"  min: {yaml_list(volume_min)}",
        f"  max: {yaml_list(volume_max)}",
        f"  resolution: {yaml_list(resolution)}",
    ]
    if not no_color:
        voxel_lines += ["", "color:", f"  methods: {yaml_list(color_methods)}"]
    voxel_config.write_text("\n".join(voxel_lines) + "\n", encoding="utf-8")


def write_batch_config(
    *,
    batch_config: Path,
    output_dir: Path,
    workers: int,
    objects: list[BatchObjectConfig],
) -> None:
    if workers < 1:
        raise ValueError("--workers must be at least 1")
    if not objects:
        raise ValueError("Batch config needs at least one object")

    # The C++ binary reads this file directly and handles object-level workers.
    batch_config.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "schema: covoca.branch1.voxel_carving_batch.v1",
        f"workers: {workers}",
        f"output_dir: {output_dir.resolve().as_posix()}",
        "",
        "objects:",
    ]

    for entry in objects:
        lines += [
            f"  - name: {entry.name}",
            f"    object_config: {entry.object_config.resolve().as_posix()}",
        ]
        lines += [
            "    voxel_grid:",
            f"      min: {yaml_list(entry.volume_min)}",
            f"      max: {yaml_list(entry.volume_max)}",
            f"      resolution: {yaml_list(entry.resolution)}",
        ]
        if not entry.no_color:
            lines += [
                "    color:",
                f"      methods: {yaml_list(entry.color_methods)}",
            ]

    batch_config.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--object", required=True, help="object folder name")
    parser.add_argument(
        "--objects-root",
        type=Path,
        default=OBJECTS_ROOT,
        help="root containing one subfolder per object",
    )
    parser.add_argument("--object-config", type=Path)
    parser.add_argument("--voxel-config", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--foreground-threshold", type=int, default=1)
    parser.add_argument(
        "--volume-min", nargs=3, type=float, default=[-0.05, 0.0, -0.05]
    )
    parser.add_argument("--volume-max", nargs=3, type=float, default=[0.05, 0.10, 0.05])
    parser.add_argument("--resolution", nargs=3, type=int, default=[100, 100, 100])
    parser.add_argument(
        "--color-methods", nargs="+", choices=COLOR_METHODS, default=["average"]
    )
    parser.add_argument("--no-color", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    object_config = args.object_config or CONFIGS / f"{args.object}.object.yaml"
    voxel_config = args.voxel_config or CONFIGS / f"{args.object}.voxel_carving.yaml"
    output_dir = args.output_dir or RESULTS / args.object
    write_configs(
        object_name=args.object,
        objects_root=args.objects_root,
        object_config=object_config,
        voxel_config=voxel_config,
        output_dir=output_dir,
        foreground_threshold=args.foreground_threshold,
        volume_min=args.volume_min,
        volume_max=args.volume_max,
        resolution=args.resolution,
        color_methods=args.color_methods,
        no_color=args.no_color,
    )
    print(f"Wrote {object_config}")
    print(f"Wrote {voxel_config}")
    print(f"Output: {output_dir}")
    print(f"Run: ./build/main {object_config} {voxel_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
