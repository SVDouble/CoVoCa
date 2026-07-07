#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# ///
"""Create dataset and voxel-carving YAML configs for the C++ loader."""

import argparse
from pathlib import Path

DATASETS = Path("local/datasets")
CONFIGS = Path("local/configs")
COLOR_METHODS = ("average", "best_view", "weighted_average", "median")


def yaml_list(values: list[float] | list[int] | list[str]) -> str:
    return "[" + ", ".join(map(str, values)) + "]"


def write_configs(
    *,
    dataset: str,
    datasets_root: Path,
    masks_root: Path,
    camera_root: Path,
    dataset_config: Path,
    voxel_config: Path,
    foreground_threshold: int,
    volume_min: list[float],
    volume_max: list[float],
    resolution: list[int],
    color_methods: list[str],
    no_color: bool,
) -> None:
    dataset_config.parent.mkdir(parents=True, exist_ok=True)
    voxel_config.parent.mkdir(parents=True, exist_ok=True)

    images = datasets_root / dataset / "images"
    masks = masks_root / dataset / "masks"
    camera = camera_root / dataset / "camera"

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

    color_section = ""
    if not no_color:
        color_section = f"""
color:
  methods: {yaml_list(color_methods)}
"""

    dataset_config.write_text(
        f"""schema: covoca.branch1.dataset.v1
name: {dataset}

paths:
  images_dir: {images.resolve().as_posix()}
  masks_dir: {masks.resolve().as_posix()}
  camera_dir: {camera.resolve().as_posix()}

foreground_threshold: {foreground_threshold}
""",
        encoding="utf-8",
    )
    voxel_config.write_text(
        f"""schema: covoca.branch1.voxel_carving.v1
name: {dataset}

voxel_grid:
  min: {yaml_list(volume_min)}
  max: {yaml_list(volume_max)}
  resolution: {yaml_list(resolution)}
{color_section}""",
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--datasets-root", type=Path, default=DATASETS)
    parser.add_argument("--masks-root", type=Path, default=DATASETS)
    parser.add_argument("--camera-root", type=Path, default=DATASETS)
    parser.add_argument("--dataset-config", type=Path)
    parser.add_argument("--voxel-config", type=Path)
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
    dataset_config = args.dataset_config or CONFIGS / f"{args.dataset}.dataset.yaml"
    voxel_config = args.voxel_config or CONFIGS / f"{args.dataset}.voxel_carving.yaml"
    write_configs(
        dataset=args.dataset,
        datasets_root=args.datasets_root,
        masks_root=args.masks_root,
        camera_root=args.camera_root,
        dataset_config=dataset_config,
        voxel_config=voxel_config,
        foreground_threshold=args.foreground_threshold,
        volume_min=args.volume_min,
        volume_max=args.volume_max,
        resolution=args.resolution,
        color_methods=args.color_methods,
        no_color=args.no_color,
    )
    print(f"Wrote {dataset_config}")
    print(f"Wrote {voxel_config}")
    print(f"Run: ./build/main {dataset_config} {voxel_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
