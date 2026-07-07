#!/usr/bin/env python3
# /// script
# requires-python = ">=3.14"
# ///
"""Generate all color-method mesh variants for every local object."""

from __future__ import annotations

import argparse
import math
import os
import shutil
import subprocess
from datetime import datetime
from pathlib import Path

from create_loader_config import BatchObjectConfig, write_batch_config, write_configs

ROOT = Path(__file__).resolve().parents[1]
OBJECTS_ROOT = ROOT / "local/datasets"
RESULTS = ROOT / "local/results"
METHODS = ("average", "best_view", "weighted_average", "median")
DEFAULT_WORKERS = min(4, os.cpu_count() or 1)


def object_names(root: Path, selected: list[str] | None) -> list[str]:
    keep = set(selected or [])
    names = sorted(path.name for path in root.iterdir() if (path / "images").is_dir())
    names = [name for name in names if not keep or name in keep]
    if not names:
        raise FileNotFoundError(f"No object folders with images/ found under {root}")
    return names


def latest_reference() -> Path:
    candidates = sorted(
        RESULTS.glob("mesh_variants_*"), key=lambda path: path.stat().st_mtime
    )
    if not candidates:
        raise FileNotFoundError("No local/results/mesh_variants_* reference run found")
    return candidates[-1]


def read_grid(path: Path) -> tuple[list[float], list[float], list[int]]:
    lower: list[float] | None = None
    upper: list[float] | None = None
    resolution: list[int] | None = None

    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        for key in ("min", "max", "resolution"):
            prefix = f"{key}:"
            if stripped.startswith(prefix):
                items = [
                    item.strip()
                    for item in stripped.removeprefix(prefix).strip(" []").split(",")
                ]
                if key == "min":
                    lower = [float(item) for item in items]
                elif key == "max":
                    upper = [float(item) for item in items]
                else:
                    resolution = [int(item) for item in items]

    if lower is None or upper is None or resolution is None:
        raise ValueError(f"Could not read voxel_grid from {path}")
    return lower, upper, resolution


def read_ply_bbox(path: Path) -> tuple[list[float], list[float]]:
    with path.open(encoding="utf-8") as stream:
        vertex_count = 0
        for line in stream:
            parts = line.split()
            if len(parts) == 3 and parts[:2] == ["element", "vertex"]:
                vertex_count = int(parts[2])
            elif line.strip() == "end_header":
                break

        lower = [math.inf, math.inf, math.inf]
        upper = [-math.inf, -math.inf, -math.inf]
        for _ in range(vertex_count):
            xyz = [float(value) for value in stream.readline().split()[:3]]
            for index, value in enumerate(xyz):
                lower[index] = min(lower[index], value)
                upper[index] = max(upper[index], value)
    return lower, upper


def expanded_grid(
    lower: list[float],
    upper: list[float],
    resolution: list[int],
    bbox: tuple[list[float], list[float]] | None,
    scale: float,
    margin_voxels: float,
    expand_fraction: float,
    expand_min: float,
) -> tuple[list[float], list[float], list[int], list[str]]:
    new_lower = lower.copy()
    new_upper = upper.copy()
    changes: list[str] = []
    if bbox:
        bbox_lower, bbox_upper = bbox
        for axis, name in enumerate(("x", "y", "z")):
            span = upper[axis] - lower[axis]
            step = span / max(1, resolution[axis] - 1)
            pad = max(expand_min, span * expand_fraction)
            low_margin = (bbox_lower[axis] - lower[axis]) / step
            high_margin = (upper[axis] - bbox_upper[axis]) / step
            if low_margin < margin_voxels:
                new_lower[axis] -= pad
                changes.append(f"{name}min")
            if high_margin < margin_voxels:
                new_upper[axis] += pad
                changes.append(f"{name}max")

    new_resolution = []
    for axis in range(3):
        old_span = upper[axis] - lower[axis]
        new_span = new_upper[axis] - new_lower[axis]
        new_resolution.append(
            max(2, math.ceil(resolution[axis] * scale * new_span / old_span))
        )
    return new_lower, new_upper, new_resolution, changes


def run(cmd: list[object], cwd: Path, log: Path) -> None:
    print("+ " + " ".join(map(str, cmd)), flush=True)
    with log.open("w", encoding="utf-8") as stream:
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
        if process.wait():
            raise subprocess.CalledProcessError(process.returncode, cmd)


def complete(object_dir: Path, methods: tuple[str, ...]) -> bool:
    if len(methods) == 1:
        return (object_dir / "voxel_hull.ply").is_file()
    return all((object_dir / method / "voxel_hull.ply").is_file() for method in methods)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--objects-root",
        type=Path,
        default=OBJECTS_ROOT,
        help="root containing one subfolder per object",
    )
    parser.add_argument("--reference-results", type=Path, default=None)
    parser.add_argument("--output-root", type=Path, default=None)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/main")
    parser.add_argument(
        "--object",
        action="append",
        help="process only this object; omit to process all objects",
    )
    parser.add_argument("--methods", nargs="+", choices=METHODS, default=list(METHODS))
    parser.add_argument("--resolution-scale", type=float, default=2.0)
    parser.add_argument("--boundary-margin-voxels", type=float, default=8.0)
    parser.add_argument("--expand-fraction", type=float, default=0.10)
    parser.add_argument("--expand-min", type=float, default=0.01)
    parser.add_argument(
        "--workers",
        type=int,
        default=DEFAULT_WORKERS,
        help="number of objects to carve in parallel",
    )
    parser.add_argument("--resume", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.workers < 1:
        raise ValueError("--workers must be at least 1")
    objects_root = args.objects_root.resolve()
    reference = (args.reference_results or latest_reference()).resolve()
    output_root = (
        args.output_root or RESULTS / f"mesh_variants_{datetime.now():%Y%m%d_%H%M%S}"
    ).resolve()
    binary = args.binary.resolve()
    methods = tuple(args.methods)
    names = object_names(objects_root, args.object)
    if not binary.is_file():
        raise FileNotFoundError(f"Voxel carving binary not found: {binary}")
    output_root.mkdir(parents=True, exist_ok=True)
    batch_objects: list[BatchObjectConfig] = []

    for index, object_name in enumerate(names, 1):
        print(f"[{index}/{len(names)}] {object_name}", flush=True)
        object_output = output_root / object_name
        object_output.mkdir(parents=True, exist_ok=True)
        if args.resume and complete(object_output, methods):
            print("  already complete", flush=True)
            continue

        source_config = reference / object_name / "voxel_carving.source.yaml"
        lower, upper, resolution = read_grid(source_config)
        hull = reference / object_name / "average" / "voxel_hull.ply"
        bbox = read_ply_bbox(hull) if hull.is_file() else None
        lower, upper, resolution, expanded = expanded_grid(
            lower,
            upper,
            resolution,
            bbox,
            args.resolution_scale,
            args.boundary_margin_voxels,
            args.expand_fraction,
            args.expand_min,
        )
        if expanded:
            print(f"  expanded: {', '.join(expanded)}", flush=True)
        print(f"  resolution: {resolution}", flush=True)

        object_config = object_output / "object.source.yaml"
        voxel_config = object_output / "voxel_carving_methods.yaml"
        write_configs(
            object_name=object_name,
            objects_root=objects_root,
            object_config=object_config,
            voxel_config=voxel_config,
            output_dir=object_output,
            foreground_threshold=1,
            volume_min=lower,
            volume_max=upper,
            resolution=resolution,
            color_methods=list(methods),
            no_color=False,
        )
        shutil.copy2(voxel_config, object_output / "voxel_carving.source.yaml")
        batch_objects.append(
            BatchObjectConfig(
                name=object_name,
                object_config=object_config,
                volume_min=lower,
                volume_max=upper,
                resolution=resolution,
                color_methods=list(methods),
                no_color=False,
            )
        )

    if batch_objects:
        batch_config = output_root / "voxel_carving_batch.yaml"
        # Run the C++ binary once; it distributes objects according to workers.
        write_batch_config(
            batch_config=batch_config,
            output_dir=output_root,
            workers=args.workers,
            objects=batch_objects,
        )
        run([binary, batch_config], cwd=ROOT, log=output_root / "voxel_carving.log")

    print(f"Output: {output_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
