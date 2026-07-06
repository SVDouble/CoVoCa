#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# ///
"""Create dataset and voxel-carving YAML configs for the C++ loader."""

import argparse
from pathlib import Path

DATASETS = Path("local/datasets")
ANNOTATIONS = Path("datasets/annotations")
MASKS = ANNOTATIONS
CAMERAS = ANNOTATIONS
CONFIGS = Path("local/configs")


def annotation(root: Path, dataset: str, suffix: str, run: str | None) -> Path:
    if run:
        return root / run / dataset
    if (root / dataset / suffix).exists():
        return root / dataset
    runs = [path for path in root.iterdir() if (path / dataset / suffix).exists()]
    if not runs:
        raise FileNotFoundError(f"No generated run for {dataset!r} under {root}")
    return sorted(runs, key=lambda path: path.name)[-1] / dataset


def fmt(values: list[float] | list[int]) -> str:
    return "[" + ", ".join(map(str, values)) + "]"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--datasets-root", type=Path, default=DATASETS)
    parser.add_argument("--masks-root", type=Path, default=MASKS)
    parser.add_argument("--camera-root", type=Path, default=CAMERAS)
    parser.add_argument("--mask-run")
    parser.add_argument("--camera-run")
    parser.add_argument("--dataset-config", type=Path)
    parser.add_argument("--voxel-config", type=Path)
    parser.add_argument("--foreground-threshold", type=int, default=1)
    parser.add_argument("--volume-min", nargs=3, type=float, default=[-0.05, 0.0, -0.05])
    parser.add_argument("--volume-max", nargs=3, type=float, default=[0.05, 0.10, 0.05])
    parser.add_argument("--resolution", nargs=3, type=int, default=[100, 100, 100])
    parser.add_argument("--color-method", default="average")
    parser.add_argument("--no-color", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    images = args.datasets_root / args.dataset / "images"
    masks = annotation(args.masks_root, args.dataset, "masks", args.mask_run) / "masks"
    camera = annotation(args.camera_root, args.dataset, "camera/cameras.txt", args.camera_run) / "camera" / "cameras.txt"
    for path, label, ok in ((images, "images directory", images.is_dir), (masks, "masks directory", masks.is_dir), (camera, "camera file", camera.is_file)):
        if not ok():
            raise FileNotFoundError(f"Missing {label}: {path}")

    dataset_config = args.dataset_config or CONFIGS / f"{args.dataset}.dataset.yaml"
    voxel_config = args.voxel_config or CONFIGS / f"{args.dataset}.voxel_carving.yaml"
    dataset_config.parent.mkdir(parents=True, exist_ok=True)
    voxel_config.parent.mkdir(parents=True, exist_ok=True)
    dataset_config.write_text(
        f"""schema: covoca.branch1.dataset.v1
name: {args.dataset}

paths:
  images_dir: {images.resolve().as_posix()}
  masks_dir: {masks.resolve().as_posix()}
  camera_file: {camera.resolve().as_posix()}

foreground_threshold: {args.foreground_threshold}
""",
        encoding="utf-8",
    )
    voxel_config.write_text(
        f"""schema: covoca.branch1.voxel_carving.v1
name: {args.dataset}

voxel_grid:
  min: {fmt(args.volume_min)}
  max: {fmt(args.volume_max)}
  resolution: {fmt(args.resolution)}
{"" if args.no_color else f"\ncolor:\n  method: {args.color_method}\n"}""",
        encoding="utf-8",
    )
    print(f"Wrote {dataset_config}")
    print(f"Wrote {voxel_config}")
    print(f"  images: {images}")
    print(f"  masks:  {masks}")
    print(f"  camera: {camera}")
    print(f"Run: ./build/main {dataset_config} {voxel_config}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
