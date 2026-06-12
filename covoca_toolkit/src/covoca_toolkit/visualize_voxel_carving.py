"""Render an interactive HTML dashboard summarizing a voxel-carving run.

Shows a sample of input images and their silhouette masks alongside the
carved point cloud as a rotatable 3D plot, so a run's result can be
inspected in a browser without opening the PLY/OFF files in a 3D viewer.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import plotly.graph_objects as go
import yaml
from PIL import Image
from plotly.subplots import make_subplots


def load_yaml(path: Path) -> dict:
    with open(path) as f:
        return yaml.safe_load(f)


def load_ply_vertices(path: Path) -> np.ndarray:
    """Read vertex positions from an ASCII PLY (point cloud or mesh)."""
    with open(path) as f:
        lines = f.readlines()

    vertex_count = 0
    header_end = 0
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("element vertex"):
            vertex_count = int(stripped.split()[-1])
        elif stripped == "end_header":
            header_end = i + 1
            break

    return np.array([[float(value) for value in lines[header_end + i].split()[:3]] for i in range(vertex_count)])


def pick_sample_frames(frames: list[dict], count: int) -> list[dict]:
    if count >= len(frames):
        return frames
    indices = np.linspace(0, len(frames) - 1, num=count, dtype=int)
    return [frames[i] for i in indices]


def load_thumbnail(path: Path, max_size: int = 320) -> np.ndarray:
    """Load an image and shrink it for embedding in the dashboard."""
    image = Image.open(path).convert("RGB")
    image.thumbnail((max_size, max_size))
    return np.array(image)


def build_dashboard(config_path: Path, output_path: Path, num_views: int) -> None:
    project_root = config_path.resolve().parents[2]
    config = load_yaml(config_path)
    calibration = load_yaml(project_root / config["paths"]["calibration_result"])
    masks_dir = Path(config["paths"]["masks_dir"])

    frames = [{"path": frame["image"]} for frame in calibration["frames"]]
    for frame in frames:
        frame["mask"] = str(masks_dir / Path(frame["path"]).with_suffix(".png").name)

    sample_frames = pick_sample_frames(frames, num_views)

    output_dir = project_root / config["paths"]["output_dir"]
    points = load_ply_vertices(output_dir / config["export"]["occupied_points_file"])

    fig = make_subplots(
        rows=3,
        cols=num_views,
        specs=[
            [{"type": "xy"}] * num_views,
            [{"type": "xy"}] * num_views,
            [{"type": "scene", "colspan": num_views}] + [None] * (num_views - 1),
        ],
        row_heights=[0.25, 0.25, 0.5],
        subplot_titles=[Path(frame["path"]).name for frame in sample_frames]
        + [""] * num_views
        + ["occupied voxels"],
        vertical_spacing=0.06,
    )

    for col, frame in enumerate(sample_frames):
        image = load_thumbnail(project_root / frame["path"])
        fig.add_trace(go.Image(z=image), row=1, col=col + 1)

        mask = load_thumbnail(project_root / frame["mask"])
        fig.add_trace(go.Image(z=mask), row=2, col=col + 1)

    for row in (1, 2):
        for col in range(1, num_views + 1):
            fig.update_xaxes(visible=False, row=row, col=col)
            fig.update_yaxes(visible=False, row=row, col=col)

    fig.add_trace(
        go.Scatter3d(
            x=points[:, 0],
            y=points[:, 1],
            z=points[:, 2],
            mode="markers",
            marker={"size": 1.5, "color": points[:, 2], "colorscale": "Viridis"},
        ),
        row=3,
        col=1,
    )
    fig.update_scenes(aspectmode="data")

    fig.update_layout(
        title=f"{config['name']} — {len(points)} occupied voxels",
        showlegend=False,
        height=420 * 3,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.write_html(output_path, include_plotlyjs="cdn")
    print(f"Wrote {output_path}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render an interactive HTML dashboard for a voxel-carving run.")
    parser.add_argument("--config", type=Path, required=True, help="Voxel carving config YAML")
    parser.add_argument("--output", type=Path, default=None, help="Output HTML path (default: <output_dir>/dashboard.html)")
    parser.add_argument("--num-views", type=int, default=4, help="Number of sample images to show (default: 4)")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    config = load_yaml(args.config)
    project_root = args.config.resolve().parents[2]
    output_path = args.output or (project_root / config["paths"]["output_dir"] / "dashboard.html")
    build_dashboard(args.config, output_path, args.num_views)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
