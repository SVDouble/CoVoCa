#!/usr/bin/env python3
# /// script
# requires-python = ">=3.14"
# dependencies = [
#   "numpy==2.4.6",
#   "pillow==12.2.0",
#   "pyyaml==6.0.3",
# ]
# ///
"""Build comparison panoramas from raw views, masks, and mesh variants."""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import numpy as np
import yaml
from PIL import Image, ImageDraw, ImageFont

Image.MAX_IMAGE_PIXELS = None

ROOT = Path(__file__).resolve().parents[1]
IMAGE_EXTENSIONS = (".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".ppm", ".pgm")
DEFAULT_METHODS = ("average", "best_view", "weighted_average", "median")
CameraView = dict[str, np.ndarray | str]
Mesh = tuple[np.ndarray, np.ndarray, np.ndarray]
BASE_TILE_SIZE = (320, 240)


@dataclass(frozen=True)
class TileColumn:
    view: CameraView
    original: Image.Image
    mask: Image.Image
    crop: tuple[int, int, int, int]


def scaled(value: int, scale: float) -> int:
    return max(1, int(round(value * scale)))


def latest_mesh_results() -> Path:
    candidates = sorted(
        (ROOT / "local/results").glob("mesh_variants_*"),
        key=lambda path: path.stat().st_mtime,
    )
    if not candidates:
        raise FileNotFoundError("No local/results/mesh_variants_* directory found")
    return candidates[-1]


def find_by_name_or_stem(directory: Path, name: str | Path) -> Path:
    path = Path(name)
    exact = directory / path.name
    if exact.is_file():
        return exact
    stem = path.stem
    for extension in IMAGE_EXTENSIONS:
        candidate = directory / f"{stem}{extension}"
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"Could not find {path.name} in {directory}")


def load_yaml(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        data = yaml.safe_load(stream)
    if not isinstance(data, dict):
        raise ValueError(f"Expected a YAML mapping: {path}")
    return data


def read_camera_dir(path: Path) -> list[CameraView]:
    intrinsics = load_yaml(path / "intrinsics.yaml")
    poses = load_yaml(path / "poses.yaml")
    views = []
    for frame in poses["frames"]:
        profile = intrinsics["profiles"][frame["intrinsics_profile"]]
        views.append(
            {
                "image": frame["image"],
                "intrinsics": np.asarray(profile["matrix"], dtype=np.float64),
                "rotation": np.asarray(
                    frame["rotation_board_to_camera"], dtype=np.float64
                ),
                "translation": np.asarray(
                    frame["tvec_board_to_camera_m"], dtype=np.float64
                ),
            }
        )
    return views


def read_ply(path: Path) -> Mesh:
    with path.open("r", encoding="utf-8") as stream:
        vertex_count = 0
        face_count = 0
        while True:
            line = stream.readline()
            if not line:
                raise ValueError(f"missing end_header in {path}")
            parts = line.strip().split()
            if len(parts) == 3 and parts[:2] == ["element", "vertex"]:
                vertex_count = int(parts[2])
            elif len(parts) == 3 and parts[:2] == ["element", "face"]:
                face_count = int(parts[2])
            elif line.strip() == "end_header":
                break

        vertices = np.empty((vertex_count, 3), dtype=np.float64)
        colors = np.empty((vertex_count, 3), dtype=np.uint8)
        for i in range(vertex_count):
            values = stream.readline().split()
            vertices[i] = [float(values[0]), float(values[1]), float(values[2])]
            colors[i] = [int(values[3]), int(values[4]), int(values[5])]

        faces = np.empty((face_count, 3), dtype=np.int64)
        for i in range(face_count):
            values = stream.readline().split()
            faces[i] = [int(values[1]), int(values[2]), int(values[3])]
    return vertices, colors, faces


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    paths = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
        if bold
        else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf"
        if bold
        else "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    ]
    for path in paths:
        if Path(path).is_file():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def object_crop(
    mask: Image.Image, aspect: float, margin: float
) -> tuple[int, int, int, int]:
    array = np.asarray(mask.convert("L"))
    ys, xs = np.nonzero(array > 0)
    width, height = mask.size
    if len(xs) == 0:
        return (0, 0, width, height)

    left, right = int(xs.min()), int(xs.max()) + 1
    top, bottom = int(ys.min()), int(ys.max()) + 1
    box_width = max(1, right - left)
    box_height = max(1, bottom - top)
    center_x = (left + right) / 2.0
    center_y = (top + bottom) / 2.0
    box_width *= 1.0 + margin
    box_height *= 1.0 + margin

    if box_width / box_height < aspect:
        box_width = box_height * aspect
    else:
        box_height = box_width / aspect

    left = center_x - box_width / 2.0
    right = center_x + box_width / 2.0
    top = center_y - box_height / 2.0
    bottom = center_y + box_height / 2.0

    if left < 0:
        right -= left
        left = 0
    if right > width:
        left -= right - width
        right = width
    if top < 0:
        bottom -= top
        top = 0
    if bottom > height:
        top -= bottom - height
        bottom = height

    return (
        max(0, int(math.floor(left))),
        max(0, int(math.floor(top))),
        min(width, int(math.ceil(right))),
        min(height, int(math.ceil(bottom))),
    )


def fit_crop(
    image: Image.Image, crop: tuple[int, int, int, int], size: tuple[int, int]
) -> Image.Image:
    return image.crop(crop).resize(size, Image.Resampling.LANCZOS)


def mask_tile(
    mask: Image.Image, crop: tuple[int, int, int, int], size: tuple[int, int]
) -> Image.Image:
    cropped = mask.convert("L").crop(crop).resize(size, Image.Resampling.NEAREST)
    return Image.merge("RGB", (cropped, cropped, cropped))


def render_mesh_tile(
    mesh: Mesh,
    view: CameraView,
    crop: tuple[int, int, int, int],
    size: tuple[int, int],
) -> Image.Image:
    vertices, colors, faces = mesh
    tile_width, tile_height = size
    image = Image.new("RGB", size, (246, 247, 249))
    draw = ImageDraw.Draw(image)
    if len(vertices) == 0 or len(faces) == 0:
        return image

    rotation = np.asarray(view["rotation"], dtype=np.float64)
    translation = np.asarray(view["translation"], dtype=np.float64)
    intrinsics = np.asarray(view["intrinsics"], dtype=np.float64)
    camera_points = vertices @ rotation.T + translation
    z = camera_points[:, 2]
    positive = z > 1e-9
    projected = camera_points @ intrinsics.T
    projected[:, 0] /= projected[:, 2]
    projected[:, 1] /= projected[:, 2]

    left, top, right, bottom = crop
    crop_width = max(1, right - left)
    crop_height = max(1, bottom - top)
    screen = np.empty((len(vertices), 2), dtype=np.float64)
    screen[:, 0] = (projected[:, 0] - left) * tile_width / crop_width
    screen[:, 1] = (projected[:, 1] - top) * tile_height / crop_height

    face_positive = positive[faces].all(axis=1)
    if not np.any(face_positive):
        return image

    valid_faces = faces[face_positive]
    mean_depth = z[valid_faces].mean(axis=1)
    face_colors = colors[valid_faces].mean(axis=1).astype(np.float64)

    tri_camera = camera_points[valid_faces]
    normals = np.cross(
        tri_camera[:, 1] - tri_camera[:, 0], tri_camera[:, 2] - tri_camera[:, 0]
    )
    normal_lengths = np.linalg.norm(normals, axis=1)
    valid_normals = normal_lengths > 1e-12
    normals[valid_normals] /= normal_lengths[valid_normals, None]
    light = np.array([0.2, -0.4, 1.0])
    light /= np.linalg.norm(light)
    shade = 0.58 + 0.42 * np.maximum(normals @ light, 0.0)
    face_colors = np.clip(face_colors * shade[:, None], 0, 255).astype(np.uint8)

    for order_index in np.argsort(mean_depth)[::-1]:
        polygon = [tuple(point) for point in screen[valid_faces[order_index]]]
        draw.polygon(
            polygon, fill=tuple(int(channel) for channel in face_colors[order_index])
        )

    return image


def column_data(
    object_dir: Path, tile_size: tuple[int, int], margin: float
) -> list[TileColumn]:
    views = read_camera_dir(object_dir / "camera")
    images_dir = object_dir / "images"
    columns = []
    aspect = tile_size[0] / tile_size[1]
    for view in views:
        image_path = find_by_name_or_stem(images_dir, str(view["image"]))
        mask_path = find_by_name_or_stem(object_dir / "masks", image_path.name)
        with Image.open(image_path) as source:
            original = source.convert("RGB")
        with Image.open(mask_path) as source:
            mask = source.convert("L")
        crop = object_crop(mask, aspect, margin)
        columns.append(
            TileColumn(
                view=view,
                original=fit_crop(original, crop, tile_size),
                mask=mask_tile(mask, crop, tile_size),
                crop=crop,
            )
        )
    return columns


def draw_label_center(
    draw: ImageDraw.ImageDraw,
    box: tuple[int, int, int, int],
    text: str,
    fill: tuple[int, int, int],
    font_obj,
) -> None:
    text_box = draw.textbbox((0, 0), text, font=font_obj)
    x = box[0] + ((box[2] - box[0]) - (text_box[2] - text_box[0])) / 2
    y = box[1] + ((box[3] - box[1]) - (text_box[3] - text_box[1])) / 2
    draw.text((x, y), text, fill=fill, font=font_obj)


def build_panorama(
    object_name: str,
    columns: list[TileColumn],
    meshes: dict[str, Mesh],
    methods: tuple[str, ...],
    tile_size: tuple[int, int],
    label_scale: float,
) -> Image.Image:
    tile_width, tile_height = tile_size
    ui_scale = max(
        0.1,
        label_scale
        * min(tile_width / BASE_TILE_SIZE[0], tile_height / BASE_TILE_SIZE[1]),
    )
    label_width = scaled(190, ui_scale)
    title_height = scaled(48, ui_scale)
    column_label_height = scaled(32, ui_scale)
    line_width = scaled(1, ui_scale)
    rows = ("original", "mask", *methods)
    width = label_width + len(columns) * tile_width
    height = title_height + column_label_height + len(rows) * tile_height

    canvas = Image.new("RGB", (width, height), (238, 240, 243))
    draw = ImageDraw.Draw(canvas)
    title_font = font(scaled(24, ui_scale), bold=True)
    label_font = font(scaled(17, ui_scale), bold=True)
    small_font = font(scaled(12, ui_scale))

    draw.rectangle((0, 0, width, title_height), fill=(32, 36, 41))
    draw.text(
        (scaled(16, ui_scale), scaled(10, ui_scale)),
        f"{object_name} - {len(columns)} views",
        fill=(255, 255, 255),
        font=title_font,
    )

    y0 = title_height
    for col_index, _ in enumerate(columns):
        x = label_width + col_index * tile_width
        draw.rectangle(
            (x, y0, x + tile_width, y0 + column_label_height), fill=(222, 226, 231)
        )
        draw_label_center(
            draw,
            (x, y0, x + tile_width, y0 + column_label_height),
            f"view {col_index + 1:02d}",
            (45, 49, 56),
            small_font,
        )

    for row_index, row in enumerate(rows):
        y = title_height + column_label_height + row_index * tile_height
        draw.rectangle((0, y, label_width, y + tile_height), fill=(222, 226, 231))
        draw_label_center(
            draw, (0, y, label_width, y + tile_height), row, (35, 39, 45), label_font
        )
        for col_index, column in enumerate(columns):
            x = label_width + col_index * tile_width
            if row == "original":
                tile = column.original
            elif row == "mask":
                tile = column.mask
            else:
                tile = render_mesh_tile(
                    meshes[row], column.view, column.crop, tile_size
                )
            canvas.paste(tile, (x, y))

    for col_index in range(len(columns) + 1):
        x = label_width + col_index * tile_width
        draw.line((x, title_height, x, height), fill=(210, 214, 220), width=line_width)
    for row_index in range(len(rows) + 1):
        y = title_height + column_label_height + row_index * tile_height
        draw.line((0, y, width, y), fill=(210, 214, 220), width=line_width)

    return canvas


def save_combined(panoramas: list[Path], output: Path, scale: float) -> None:
    sizes = []
    for path in panoramas:
        with Image.open(path) as image:
            sizes.append(
                (max(1, int(image.width * scale)), max(1, int(image.height * scale)))
            )

    gap = max(1, int(round(28 * scale)))
    width = max(width for width, _ in sizes)
    height = sum(height for _, height in sizes) + gap * (len(sizes) - 1)
    combined = Image.new("RGB", (width, height), (238, 240, 243))
    y = 0
    for path, size in zip(panoramas, sizes):
        with Image.open(path) as source:
            image = source.convert("RGB")
        if image.size != size:
            image = image.resize(size, Image.Resampling.LANCZOS)
        combined.paste(image, (0, y))
        image.close()
        y += size[1] + gap
    combined.save(output, quality=92)


def scale_label(scale: float) -> str:
    return f"{scale:g}".replace(".", "p")


def has_panorama_inputs(path: Path) -> bool:
    return (
        (path / "images").is_dir()
        and (path / "masks").is_dir()
        and (path / "camera/intrinsics.yaml").is_file()
        and (path / "camera/poses.yaml").is_file()
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--mesh-results", type=Path, default=None, help="mesh variant result root"
    )
    parser.add_argument(
        "--objects-root",
        type=Path,
        default=ROOT / "local/datasets",
        help="root containing one subfolder per object",
    )
    parser.add_argument("--output-root", type=Path, default=None)
    parser.add_argument("--methods", nargs="+", default=list(DEFAULT_METHODS))
    parser.add_argument("--tile-width", type=int, default=320)
    parser.add_argument("--tile-height", type=int, default=240)
    parser.add_argument(
        "--label-scale",
        type=float,
        default=1.5,
        help="scale labels relative to tile size",
    )
    parser.add_argument("--crop-margin", type=float, default=0.35)
    parser.add_argument("--combined-scale", type=float, default=0.5)
    parser.add_argument("--combined-scales", nargs="+", type=float, default=None)
    parser.add_argument("--no-combined", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    mesh_results = (args.mesh_results or latest_mesh_results()).resolve()
    default_output = (
        ROOT / "local/results" / f"mesh_panoramas_{datetime.now():%Y%m%d_%H%M%S}"
    )
    output_root = (args.output_root or default_output).resolve()
    methods = tuple(args.methods)
    tile_size = (args.tile_width, args.tile_height)
    output_root.mkdir(parents=True, exist_ok=True)

    objects = sorted(
        path.name for path in args.objects_root.iterdir() if has_panorama_inputs(path)
    )
    panorama_paths = []
    for index, object_name in enumerate(objects, 1):
        print(f"[{index}/{len(objects)}] {object_name}", flush=True)
        object_dir = args.objects_root / object_name
        columns = column_data(object_dir, tile_size, args.crop_margin)
        meshes = {
            method: read_ply(mesh_results / object_name / method / "voxel_hull.ply")
            for method in methods
        }
        panorama = build_panorama(
            object_name,
            columns,
            meshes,
            methods,
            tile_size,
            args.label_scale,
        )
        output = output_root / f"{object_name}_panorama.png"
        panorama.save(output)
        panorama_paths.append(output)

    if not args.no_combined and panorama_paths:
        scales = args.combined_scales or [args.combined_scale]
        for scale in scales:
            filename = "all_objects_panorama.jpg"
            if len(scales) > 1:
                filename = f"all_objects_panorama_scale_{scale_label(scale)}.jpg"
            output = output_root / filename
            save_combined(panorama_paths, output, scale)

    print(f"Output: {output_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
