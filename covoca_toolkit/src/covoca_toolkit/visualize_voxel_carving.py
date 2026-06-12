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


def load_ply_vertices(path: Path) -> tuple[np.ndarray, np.ndarray | None]:
    """Read vertex positions (and colors, if present) from an ASCII PLY.

    Returns:
        A `(positions, colors)` tuple. `positions` is `(N, 3)` float64.
        `colors` is `(N, 3)` uint8 in `[0, 255]` if the PLY has `red`,
        `green`, and `blue` vertex properties, otherwise `None`.
    """
    with open(path) as f:
        lines = f.readlines()

    vertex_count = 0
    properties: list[str] = []
    header_end = 0
    in_vertex_element = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("element vertex"):
            vertex_count = int(stripped.split()[-1])
            in_vertex_element = True
        elif stripped.startswith("element"):
            in_vertex_element = False
        elif stripped.startswith("property") and in_vertex_element:
            properties.append(stripped.split()[-1])
        elif stripped == "end_header":
            header_end = i + 1
            break

    rows = [lines[header_end + i].split() for i in range(vertex_count)]
    positions = np.array([[float(value) for value in row[:3]] for row in rows])

    colors = None
    if {"red", "green", "blue"}.issubset(properties):
        color_indices = [properties.index(name) for name in ("red", "green", "blue")]
        colors = np.array([[int(row[i]) for i in color_indices] for row in rows], dtype=np.uint8)

    return positions, colors


def project_points_to_pixels(
    points: np.ndarray, frame: dict, camera_matrix: list[list[float]]
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Project world-space points into a frame's image, via the pinhole model.

    Mirrors `CalibratedView::projectWorldPoint` (no distortion correction).

    Returns:
        `(pixel_x, pixel_y, depth)`, each `(N,)`. Points with `depth <= 0` are
        behind the camera; their pixel coordinates are undefined.
    """
    rotation = np.array(frame["rotation_board_to_camera"])
    translation = np.array(frame["tvec_board_to_camera_m"])
    points_camera = points @ rotation.T + translation
    depth = points_camera[:, 2]

    fx, fy = camera_matrix[0][0], camera_matrix[1][1]
    cx, cy = camera_matrix[0][2], camera_matrix[1][2]
    safe_depth = np.where(depth > 0, depth, 1.0)
    pixel_x = fx * points_camera[:, 0] / safe_depth + cx
    pixel_y = fy * points_camera[:, 1] / safe_depth + cy
    return pixel_x, pixel_y, depth


def render_point_cloud_view(
    points: np.ndarray,
    colors: np.ndarray,
    frame: dict,
    camera_matrix: list[list[float]],
    width: int,
    height: int,
    voxel_size_m: float,
) -> tuple[np.ndarray, np.ndarray]:
    """Rasterizes the colored point cloud as seen from one calibrated view.

    Projects every point with `project_points_to_pixels`, keeps points in
    front of the camera and inside the image, and paints each onto a pixel
    grid (nearer points last, so they win over farther ones at the same
    pixel). Each point is splatted onto a square sized to match its voxel's
    projected footprint (`focal_length * voxel_size_m / depth` pixels), so
    neighboring voxels project to contiguous pixels instead of sparse dots.

    Returns:
        `(image, mask)`: `image` is `(height, width, 3)` uint8 (black where
        nothing was painted); `mask` is `(height, width)` bool, true where a
        point was painted.
    """
    pixel_x, pixel_y, depth = project_points_to_pixels(points, frame, camera_matrix)
    visible = (depth > 0) & (pixel_x >= 0) & (pixel_x < width) & (pixel_y >= 0) & (pixel_y < height)

    order = np.argsort(-depth[visible])
    xs = pixel_x[visible][order].astype(int)
    ys = pixel_y[visible][order].astype(int)
    zs = depth[visible][order]
    cols = colors[visible][order]

    focal_length = 0.5 * (camera_matrix[0][0] + camera_matrix[1][1])
    radii = np.maximum(1, np.round(0.5 * focal_length * voxel_size_m / zs)).astype(int)

    image = np.zeros((height, width, 3), dtype=np.uint8)
    mask = np.zeros((height, width), dtype=bool)
    for dx in range(-radii.max(), radii.max() + 1):
        for dy in range(-radii.max(), radii.max() + 1):
            covered = (abs(dx) <= radii) & (abs(dy) <= radii)
            x = np.clip(xs[covered] + dx, 0, width - 1)
            y = np.clip(ys[covered] + dy, 0, height - 1)
            image[y, x] = cols[covered]
            mask[y, x] = True
    return image, mask


def color_difference_heatmap(rendered: np.ndarray, mask: np.ndarray, source: np.ndarray) -> np.ndarray:
    """Computes a per-pixel mean absolute color difference.

    Args:
        rendered: `(H, W, 3)` uint8 image rendered from the point cloud.
        mask: `(H, W)` bool, true where `rendered` has a painted pixel.
        source: `(H, W, 3)` uint8 source photo, same resolution as `rendered`.

    Returns:
        `(H, W)` float array of `mean(|rendered - source|)` over channels,
        `NaN` where `mask` is false (nothing to compare).
    """
    diff = np.abs(rendered.astype(np.int16) - source.astype(np.int16)).mean(axis=2)
    return np.where(mask, diff, np.nan)


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


def load_image(path: Path) -> np.ndarray:
    """Load an image at full resolution as `(H, W, 3)` uint8 RGB."""
    return np.array(Image.open(path).convert("RGB"))


def build_camera_traces(frames: list[dict], axis_length: float) -> tuple[go.Scatter3d, go.Scatter3d]:
    """Builds 3D traces showing each camera's position and viewing direction.

    Args:
        frames: Calibration frames, each with `camera_center_board_m` and
            `rotation_board_to_camera`.
        axis_length: Length, in world units, of the viewing-direction line
            drawn from each camera center.

    Returns:
        `(centers_trace, directions_trace)`: a marker trace for the camera
        centers, and a line trace for their viewing directions (camera +Z
        axis in world coordinates).
    """
    centers = np.array([frame["camera_center_board_m"] for frame in frames])
    labels = [Path(frame["image"]).stem for frame in frames]

    line_x: list[float | None] = []
    line_y: list[float | None] = []
    line_z: list[float | None] = []
    for frame in frames:
        center = np.array(frame["camera_center_board_m"])
        rotation = np.array(frame["rotation_board_to_camera"])
        direction = rotation.T @ np.array([0.0, 0.0, 1.0])
        tip = center + axis_length * direction
        line_x += [center[0], tip[0], None]
        line_y += [center[1], tip[1], None]
        line_z += [center[2], tip[2], None]

    centers_trace = go.Scatter3d(
        x=centers[:, 0],
        y=centers[:, 1],
        z=centers[:, 2],
        mode="markers+text",
        marker={"size": 4, "color": "red", "symbol": "diamond"},
        text=labels,
        textposition="top center",
        textfont={"size": 9, "color": "red"},
        hovertext=labels,
        hoverinfo="text",
        name="cameras",
    )
    directions_trace = go.Scatter3d(
        x=line_x,
        y=line_y,
        z=line_z,
        mode="lines",
        line={"color": "red", "width": 2},
        name="camera view directions",
        showlegend=False,
    )
    return centers_trace, directions_trace


def build_dashboard(config_path: Path, output_path: Path, num_views: int) -> None:
    project_root = config_path.resolve().parents[2]
    config = load_yaml(config_path)
    calibration = load_yaml(project_root / config["paths"]["calibration_result"])
    masks_dir = Path(config["paths"]["masks_dir"])
    image_width = calibration["image_size"]["width"]
    image_height = calibration["image_size"]["height"]
    camera_matrix = calibration["camera"]["matrix"]

    frames = []
    for frame in calibration["frames"]:
        frames.append({
            **frame,
            "path": frame["image"],
            "mask": str(masks_dir / Path(frame["image"]).with_suffix(".png").name),
        })

    sample_frames = pick_sample_frames(frames, num_views)

    output_dir = project_root / config["paths"]["output_dir"]
    color_config = config.get("color")
    if color_config:
        points, colors = load_ply_vertices(output_dir / color_config["occupied_points_file"])
    else:
        points, colors = load_ply_vertices(output_dir / config["export"]["occupied_points_file"])

    grid_cols = max(num_views, 3)
    fig = make_subplots(
        rows=4,
        cols=grid_cols,
        specs=[
            [{"type": "xy"}] * grid_cols,
            [{"type": "xy"}] * grid_cols,
            [{"type": "scene", "colspan": grid_cols}] + [None] * (grid_cols - 1),
            [{"type": "xy"}] * 3 + [None] * (grid_cols - 3),
        ],
        row_heights=[0.2, 0.2, 0.35, 0.25],
        subplot_titles=[Path(frame["path"]).name for frame in sample_frames]
        + [""] * (grid_cols - num_views)
        + [""] * grid_cols
        + ["occupied voxels + cameras"]
        + ["source image", "rendered from same view", "color difference"],
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

    if colors is not None:
        marker = {
            "size": 1.5,
            "color": [f"rgb({r}, {g}, {b})" for r, g, b in colors],
        }
    else:
        marker = {"size": 1.5, "color": points[:, 2], "colorscale": "Viridis"}

    fig.add_trace(
        go.Scatter3d(
            x=points[:, 0],
            y=points[:, 1],
            z=points[:, 2],
            mode="markers",
            marker=marker,
            name="occupied voxels",
        ),
        row=3,
        col=1,
    )

    volume_extent = np.array(config["volume"]["max_m"]) - np.array(config["volume"]["min_m"])
    centers_trace, directions_trace = build_camera_traces(frames, axis_length=0.5 * float(np.mean(volume_extent)))
    fig.add_trace(centers_trace, row=3, col=1)
    fig.add_trace(directions_trace, row=3, col=1)

    fig.update_scenes(aspectmode="data")

    # Per-view comparison: render the colored point cloud from each frame's
    # camera and compare against that frame's source image. Covers every
    # frame (not just the thumbnail samples), selectable via the dropdown.
    fixed_trace_count = len(fig.data)
    if colors is not None:
        for frame in frames:
            source = load_image(project_root / frame["path"])
            rendered, mask = render_point_cloud_view(
                points, colors, frame, camera_matrix, image_width, image_height, config["volume"]["voxel_size_m"]
            )
            diff = color_difference_heatmap(rendered, mask, source)

            fig.add_trace(go.Image(z=source, visible=False), row=4, col=1)
            fig.add_trace(go.Image(z=rendered, visible=False), row=4, col=2)
            fig.add_trace(
                go.Heatmap(z=np.flipud(diff), colorscale="Hot", showscale=False, visible=False), row=4, col=3
            )

        for row, col in ((4, 1), (4, 2), (4, 3)):
            fig.update_xaxes(visible=False, row=row, col=col)
            fig.update_yaxes(visible=False, row=row, col=col)
        fig.update_yaxes(scaleanchor="x", scaleratio=1, row=4, col=3)

        # Make the first frame's comparison traces visible by default.
        for trace_index in range(fixed_trace_count, fixed_trace_count + 3):
            fig.data[trace_index].visible = True

        buttons = []
        for view_index, frame in enumerate(frames):
            visible = [True] * fixed_trace_count
            for other_index in range(len(frames)):
                visible += [other_index == view_index] * 3
            buttons.append({
                "label": Path(frame["path"]).name,
                "method": "update",
                "args": [{"visible": visible}],
            })

        fig.update_layout(
            updatemenus=[{
                "buttons": buttons,
                "direction": "down",
                "x": 0.0,
                "y": 0.18,
                "xanchor": "left",
                "yanchor": "bottom",
            }]
        )

    fig.update_layout(
        title=f"{config['name']} — {len(points)} occupied voxels",
        showlegend=False,
        height=420 * 4,
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
