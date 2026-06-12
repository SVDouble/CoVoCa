"""Render an interactive HTML dashboard summarizing a voxel-carving run."""

from __future__ import annotations

import argparse
import html
import json
import shutil
from pathlib import Path

import numpy as np
import plotly.graph_objects as go
import plotly.io as pio
import yaml
from PIL import Image


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


def load_ply_mesh(path: Path) -> tuple[np.ndarray, np.ndarray | None, np.ndarray]:
    """Read vertex positions, vertex colors, and triangle faces from an ASCII PLY.

    Returns:
        A `(positions, colors, triangles)` tuple. `positions` is `(N, 3)`
        float64, `colors` is `(N, 3)` uint8 (or `None` if absent), and
        `triangles` is `(M, 3)` int, indexing into `positions`.
    """
    with open(path) as f:
        lines = f.readlines()

    vertex_count = 0
    face_count = 0
    properties: list[str] = []
    header_end = 0
    element = None
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("element vertex"):
            vertex_count = int(stripped.split()[-1])
            element = "vertex"
        elif stripped.startswith("element face"):
            face_count = int(stripped.split()[-1])
            element = "face"
        elif stripped.startswith("element"):
            element = None
        elif stripped.startswith("property") and element == "vertex":
            properties.append(stripped.split()[-1])
        elif stripped == "end_header":
            header_end = i + 1
            break

    vertex_rows = [lines[header_end + i].split() for i in range(vertex_count)]
    positions = np.array([[float(value) for value in row[:3]] for row in vertex_rows])

    colors = None
    if {"red", "green", "blue"}.issubset(properties):
        color_indices = [properties.index(name) for name in ("red", "green", "blue")]
        colors = np.array([[int(row[i]) for i in color_indices] for row in vertex_rows], dtype=np.uint8)

    face_start = header_end + vertex_count
    face_rows = [lines[face_start + i].split() for i in range(face_count)]
    triangles = np.array([[int(value) for value in row[1:4]] for row in face_rows], dtype=int)

    return positions, colors, triangles


def build_voxel_mesh_trace(vertices: np.ndarray, colors: np.ndarray | None, triangles: np.ndarray) -> go.Mesh3d:
    """Builds a `go.Mesh3d` trace rendering the carved voxel cubes."""
    mesh_kwargs: dict = {}
    if colors is not None:
        mesh_kwargs["vertexcolor"] = [f"rgb({r}, {g}, {b})" for r, g, b in colors]
    else:
        mesh_kwargs["intensity"] = vertices[:, 2]
        mesh_kwargs["colorscale"] = "Viridis"
        mesh_kwargs["showscale"] = False

    return go.Mesh3d(
        x=vertices[:, 0],
        y=vertices[:, 1],
        z=vertices[:, 2],
        i=triangles[:, 0],
        j=triangles[:, 1],
        k=triangles[:, 2],
        flatshading=True,
        name="occupied voxels",
        **mesh_kwargs,
    )


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
    if not np.any(visible):
        return np.zeros((height, width, 3), dtype=np.uint8), np.zeros((height, width), dtype=bool)

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


def color_error_score(rendered: np.ndarray, source: np.ndarray) -> np.ndarray:
    """Computes a lighting-tolerant per-pixel appearance error.

    The score mixes chroma error with a smaller luma term. That makes color
    mismatches, such as the green collar rendered as wood, stand out more than
    ordinary shading differences.
    """
    rendered_float = rendered.astype(np.float32) / 255.0
    source_float = source.astype(np.float32) / 255.0

    rendered_sum = rendered_float.sum(axis=2, keepdims=True)
    source_sum = source_float.sum(axis=2, keepdims=True)
    rendered_chroma = rendered_float / np.maximum(rendered_sum, 1e-6)
    source_chroma = source_float / np.maximum(source_sum, 1e-6)

    chroma_error = np.linalg.norm(rendered_chroma - source_chroma, axis=2) * 255.0
    rendered_luma = (
        0.2126 * rendered_float[:, :, 0] + 0.7152 * rendered_float[:, :, 1] + 0.0722 * rendered_float[:, :, 2]
    )
    source_luma = 0.2126 * source_float[:, :, 0] + 0.7152 * source_float[:, :, 1] + 0.0722 * source_float[:, :, 2]
    luma_error = np.abs(rendered_luma - source_luma) * 255.0
    return (0.8 * chroma_error) + (0.2 * luma_error)


def load_image(path: Path) -> np.ndarray:
    """Load an image at full resolution as `(H, W, 3)` uint8 RGB."""
    return np.array(Image.open(path).convert("RGB"))


def load_mask(path: Path) -> np.ndarray:
    """Load a binary mask as `(H, W)` bool."""
    return np.array(Image.open(path).convert("L")) >= 128


def colorize_error_score(score: np.ndarray, mask: np.ndarray, vmin: float = 15.0, vmax: float = 45.0) -> np.ndarray:
    """Render a scalar error map as `(H, W, 3)` uint8."""
    intensity = np.clip((score - vmin) / (vmax - vmin), 0.0, 1.0)
    red = np.clip(3 * intensity, 0.0, 1.0)
    green = np.clip(3 * intensity - 1.0, 0.0, 1.0)
    blue = np.clip(3 * intensity - 2.0, 0.0, 1.0)
    image = (np.stack([red, green, blue], axis=-1) * 255).astype(np.uint8)
    image[~mask] = 0
    return image


def silhouette_error_image(source_mask: np.ndarray, render_mask: np.ndarray) -> np.ndarray:
    """Render missing source foreground in magenta and extra render coverage in cyan."""
    image = np.zeros((*source_mask.shape, 3), dtype=np.uint8)
    image[source_mask & render_mask] = (60, 60, 60)
    image[source_mask & ~render_mask] = (255, 0, 180)
    image[render_mask & ~source_mask] = (0, 190, 255)
    return image


def appearance_error_image(
    rendered: np.ndarray, render_mask: np.ndarray, source: np.ndarray, source_mask: np.ndarray
) -> np.ndarray:
    """Render color error on overlap and explicit missing/extra silhouette regions.

    Magenta means source foreground is missing from the render; cyan means the
    render has coverage outside the source mask. On overlapping foreground, a
    hot colormap shows chroma/luma mismatch.
    """
    overlap = source_mask & render_mask
    image = colorize_error_score(color_error_score(rendered, source), overlap)
    image[source_mask & ~render_mask] = (255, 0, 180)
    image[render_mask & ~source_mask] = (0, 190, 255)
    return image


def save_image_asset(image: np.ndarray, assets_dir: Path, name: str, suffix: str = "jpg", quality: int = 85) -> str:
    """Save an image sidecar and return its HTML-relative path.

    Args:
        image: RGB image as `(H, W, 3)` uint8.
        assets_dir: Dashboard asset directory next to the HTML file.
        name: File stem to write.
        suffix: Output extension, usually `"jpg"` or `"png"`.
        quality: JPEG quality.

    Returns:
        Relative path from the dashboard HTML to the saved JPEG.
    """
    assets_dir.mkdir(parents=True, exist_ok=True)
    path = assets_dir / f"{name}.{suffix}"
    if suffix == "jpg":
        Image.fromarray(image).save(path, quality=quality)
    else:
        Image.fromarray(image).save(path)
    return f"{assets_dir.name}/{path.name}"


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


def image_figure(src: str, caption: str) -> str:
    """Render one sidecar image reference as HTML."""
    return (
        "<figure>"
        f'<img src="{html.escape(src)}" loading="lazy" alt="{html.escape(caption)}">'
        f"<figcaption>{html.escape(caption)}</figcaption>"
        "</figure>"
    )


def write_dashboard_html(
    output_path: Path,
    title: str,
    plot_html: str,
    comparisons: list[dict[str, str]],
) -> None:
    """Write the dashboard shell.

    Images are normal `<img>` references to sidecar files. This keeps the HTML
    small and avoids Plotly image-trace loading issues when opening the
    dashboard from disk or serving it over HTTP.
    """
    comparison_json = json.dumps(comparisons)
    comparison_section = ""
    if comparisons:
        first = comparisons[0]
        options = "\n".join(
            f'<option value="{i}">{html.escape(item["label"])}</option>' for i, item in enumerate(comparisons)
        )
        comparison_section = f"""
    <section>
      <div class="section-header">
        <h2>View Comparison</h2>
        <div class="gallery-controls">
          <button id="previous-comparison" type="button">Previous</button>
          <select id="comparison-select">{options}</select>
          <button id="next-comparison" type="button">Next</button>
        </div>
      </div>
      <div class="comparison-grid">
        {image_figure(first["source"], "source image")}
        {image_figure(first["mask"], "mask")}
        {image_figure(first["rendered"], "rendered from same view")}
        {image_figure(first["silhouette_error"], "silhouette error")}
        {image_figure(first["appearance_error"], "missing + color error")}
      </div>
    </section>
    <script>
      const comparisons = {comparison_json};
      const select = document.getElementById("comparison-select");
      const previous = document.getElementById("previous-comparison");
      const next = document.getElementById("next-comparison");
      const figures = document.querySelectorAll(".comparison-grid figure");
      let currentComparison = 0;
      function setComparison(index) {{
        currentComparison = (index + comparisons.length) % comparisons.length;
        select.value = String(currentComparison);
        const item = comparisons[currentComparison];
        figures[0].querySelector("img").src = item.source;
        figures[1].querySelector("img").src = item.mask;
        figures[2].querySelector("img").src = item.rendered;
        figures[3].querySelector("img").src = item.silhouette_error;
        figures[4].querySelector("img").src = item.appearance_error;
      }}
      select.addEventListener("change", (event) => setComparison(Number(event.target.value)));
      previous.addEventListener("click", () => setComparison(currentComparison - 1));
      next.addEventListener("click", () => setComparison(currentComparison + 1));
    </script>
"""

    output_path.write_text(
        f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{html.escape(title)}</title>
  <style>
    body {{
      margin: 0;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: #182026;
      background: #f6f7f8;
    }}
    main {{
      width: min(1440px, calc(100% - 32px));
      margin: 0 auto;
      padding: 24px 0 36px;
    }}
    h1, h2, p {{
      margin: 0;
    }}
    h1 {{
      font-size: 28px;
      line-height: 1.2;
      margin-bottom: 20px;
    }}
    h2 {{
      font-size: 18px;
      line-height: 1.3;
    }}
    section {{
      margin-top: 20px;
    }}
    .section-header {{
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      margin-bottom: 10px;
    }}
    select {{
      min-width: 240px;
      max-width: 100%;
      padding: 6px 8px;
      border: 1px solid #b8c0c8;
      border-radius: 6px;
      background: white;
      color: inherit;
    }}
    button {{
      padding: 6px 10px;
      border: 1px solid #b8c0c8;
      border-radius: 6px;
      background: white;
      color: inherit;
      cursor: pointer;
    }}
    button:hover {{
      background: #eef2f5;
    }}
    .gallery-controls {{
      display: flex;
      align-items: center;
      flex-wrap: wrap;
      gap: 8px;
      max-width: 100%;
    }}
    .comparison-grid {{
      display: grid;
      grid-template-columns: repeat(5, minmax(0, 1fr));
      gap: 8px;
      margin-top: 8px;
    }}
    figure {{
      margin: 0;
      min-width: 0;
    }}
    img {{
      display: block;
      width: 100%;
      max-height: 520px;
      object-fit: contain;
      background: #0b0f12;
      border: 1px solid #d8dee4;
      border-radius: 6px;
    }}
    figcaption {{
      margin-top: 4px;
      color: #53606b;
      font-size: 12px;
    }}
    .plot-panel {{
      min-height: 720px;
      border: 1px solid #d8dee4;
      border-radius: 8px;
      background: white;
      overflow: hidden;
    }}
    @media (max-width: 800px) {{
      main {{
        width: min(100% - 20px, 1440px);
        padding-top: 16px;
      }}
      .comparison-grid {{
        grid-template-columns: 1fr;
      }}
      .section-header {{
        align-items: flex-start;
        flex-direction: column;
      }}
    }}
  </style>
</head>
<body>
  <main>
    <h1>{html.escape(title)}</h1>
    <section>
      <h2>Occupied Voxels + Cameras</h2>
      <div class="plot-panel">{plot_html}</div>
    </section>
    {comparison_section}
  </main>
</body>
</html>
""",
        encoding="utf-8",
    )


def build_dashboard(config_path: Path, output_path: Path) -> None:
    project_root = config_path.resolve().parents[2]
    config = load_yaml(config_path)
    calibration = load_yaml(project_root / config["paths"]["calibration_result"])
    masks_dir = Path(config["paths"]["masks_dir"])
    image_width = calibration["image_size"]["width"]
    image_height = calibration["image_size"]["height"]
    camera_matrix = calibration["camera"]["matrix"]

    frames = []
    for frame in calibration["frames"]:
        frames.append(
            {
                **frame,
                "path": frame["image"],
                "mask": str(masks_dir / Path(frame["image"]).with_suffix(".png").name),
            }
        )

    output_dir = project_root / config["paths"]["output_dir"]
    color_config = config.get("color")
    if color_config:
        points, colors = load_ply_vertices(output_dir / color_config["occupied_points_file"])
        mesh_vertices, mesh_colors, mesh_faces = load_ply_mesh(output_dir / color_config["occupied_mesh_file"])
    else:
        points, colors = load_ply_vertices(output_dir / config["export"]["occupied_points_file"])
        mesh_vertices, mesh_colors, mesh_faces = load_ply_mesh(output_dir / config["export"]["occupied_mesh_file"])

    output_path.parent.mkdir(parents=True, exist_ok=True)
    assets_dir = output_path.parent / "dashboard_assets"
    if assets_dir.exists():
        shutil.rmtree(assets_dir)

    fig = go.Figure()
    fig.add_trace(build_voxel_mesh_trace(mesh_vertices, mesh_colors, mesh_faces))
    volume_extent = np.array(config["volume"]["max_m"]) - np.array(config["volume"]["min_m"])
    centers_trace, directions_trace = build_camera_traces(frames, axis_length=0.5 * float(np.mean(volume_extent)))
    fig.add_trace(centers_trace)
    fig.add_trace(directions_trace)
    fig.update_scenes(aspectmode="data")
    fig.update_layout(
        showlegend=False,
        height=720,
        margin={"l": 0, "r": 0, "t": 8, "b": 0},
    )
    plot_html = pio.to_html(fig, include_plotlyjs="cdn", full_html=False, config={"responsive": True})

    comparisons: list[dict[str, str]] = []
    if colors is not None:
        for frame in frames:
            stem = Path(frame["path"]).stem
            source = load_image(project_root / frame["path"])
            source_mask = load_mask(project_root / frame["mask"])
            source_mask_image = np.repeat(np.where(source_mask[:, :, np.newaxis], 255, 0), 3, axis=2).astype(np.uint8)
            rendered, render_mask = render_point_cloud_view(
                points, colors, frame, camera_matrix, image_width, image_height, config["volume"]["voxel_size_m"]
            )
            silhouette_error = silhouette_error_image(source_mask, render_mask)
            appearance_error = appearance_error_image(rendered, render_mask, source, source_mask)

            comparisons.append(
                {
                    "label": Path(frame["path"]).name,
                    "source": save_image_asset(source, assets_dir, f"source_{stem}"),
                    "mask": save_image_asset(source_mask_image, assets_dir, f"comparison_mask_{stem}", suffix="png"),
                    "rendered": save_image_asset(rendered, assets_dir, f"rendered_{stem}"),
                    "silhouette_error": save_image_asset(
                        silhouette_error, assets_dir, f"silhouette_error_{stem}", suffix="png"
                    ),
                    "appearance_error": save_image_asset(
                        appearance_error, assets_dir, f"appearance_error_{stem}", suffix="png"
                    ),
                }
            )

    title = f"{config['name']} - {len(points)} occupied voxels"
    write_dashboard_html(
        output_path=output_path,
        title=title,
        plot_html=plot_html,
        comparisons=comparisons,
    )
    print(f"Wrote {output_path}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render an interactive HTML dashboard for a voxel-carving run.")
    parser.add_argument("--config", type=Path, required=True, help="Voxel carving config YAML")
    parser.add_argument(
        "--output", type=Path, default=None, help="Output HTML path (default: <output_dir>/dashboard.html)"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    config = load_yaml(args.config)
    project_root = args.config.resolve().parents[2]
    output_path = args.output or (project_root / config["paths"]["output_dir"] / "dashboard.html")
    build_dashboard(args.config, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
