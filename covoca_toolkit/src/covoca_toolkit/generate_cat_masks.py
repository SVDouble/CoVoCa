"""Generate segmentation-model silhouette masks for the cat dataset."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path
from typing import Any

import cv2
import numpy as np
from PIL import Image

DEFAULT_IMAGES_DIR = Path("local/datasets/cat/images")
DEFAULT_MASKS_DIR = Path("local/datasets/cat/masks")
DEFAULT_MODEL_PATH = Path("local/models/mobile_sam.pt")
DEFAULT_DEVICE = "cuda:0"

MIN_COMPONENT_AREA_FRACTION = 0.001
PROMPT_PADDING_FRACTION = 0.18
SEED_SATURATION_THRESHOLD = 0.22
SEED_VALUE_THRESHOLD = 0.22


def read_rgb(path: Path) -> np.ndarray:
    """Read an RGB image as uint8."""
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.uint8)


def warm_object_seed(rgb: np.ndarray) -> np.ndarray:
    """Return a conservative seed for the wooden figurine."""
    hsv = cv2.cvtColor(rgb, cv2.COLOR_RGB2HSV)
    hue = hsv[:, :, 0]
    saturation = hsv[:, :, 1] / 255.0
    value = hsv[:, :, 2] / 255.0

    red = rgb[:, :, 0].astype(np.int16)
    green = rgb[:, :, 1].astype(np.int16)
    blue = rgb[:, :, 2].astype(np.int16)

    warm_hue = (hue <= 45) | (hue >= 165)
    warm_rgb = (red >= green - 20) & (green >= blue + 4)
    seed = warm_hue & warm_rgb & (saturation >= SEED_SATURATION_THRESHOLD) & (value >= SEED_VALUE_THRESHOLD)

    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
    return cv2.morphologyEx(seed.astype(np.uint8), cv2.MORPH_OPEN, kernel).astype(bool)


def remove_border_components(mask: np.ndarray) -> np.ndarray:
    """Drop connected components touching the image border."""
    labels_count, labels, stats, _ = cv2.connectedComponentsWithStats(mask.astype(np.uint8), connectivity=8)
    keep = np.zeros_like(mask, dtype=bool)
    height, width = mask.shape
    min_area = int(height * width * MIN_COMPONENT_AREA_FRACTION)

    for label in range(1, labels_count):
        x, y, w, h, area = stats[label]
        touches_border = x == 0 or y == 0 or x + w >= width or y + h >= height
        if not touches_border and area >= min_area:
            keep |= labels == label
    return keep


def keep_largest_component(mask: np.ndarray) -> np.ndarray:
    """Keep the largest connected foreground component."""
    labels_count, labels, stats, _ = cv2.connectedComponentsWithStats(mask.astype(np.uint8), connectivity=8)
    if labels_count <= 1:
        return mask

    areas = stats[1:, cv2.CC_STAT_AREA]
    label = int(np.argmax(areas)) + 1
    return labels == label


def fill_holes(mask: np.ndarray) -> np.ndarray:
    """Fill enclosed background regions inside a binary mask."""
    filled = (mask.astype(np.uint8) * 255).copy()
    flood_mask = np.zeros((filled.shape[0] + 2, filled.shape[1] + 2), dtype=np.uint8)
    cv2.floodFill(filled, flood_mask, (0, 0), 255)
    holes = cv2.bitwise_not(filled)
    return mask | (holes > 0)


def clean_mask(mask: np.ndarray) -> np.ndarray:
    """Clean a binary foreground mask for voxel carving."""
    mask = remove_border_components(mask)
    mask = keep_largest_component(mask)

    close_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (13, 13))
    open_kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    mask = cv2.morphologyEx(mask.astype(np.uint8), cv2.MORPH_CLOSE, close_kernel).astype(bool)
    mask = fill_holes(mask)
    mask = cv2.morphologyEx(mask.astype(np.uint8), cv2.MORPH_OPEN, open_kernel).astype(bool)
    return mask


def prompt_box(seed: np.ndarray, padding_fraction: float) -> list[int]:
    """Build a SAM box prompt from the foreground seed."""
    seed = clean_mask(seed)
    if not np.any(seed):
        raise ValueError("Could not find a foreground prompt seed")

    height, width = seed.shape
    y, x = np.where(seed)
    x0, x1 = int(x.min()), int(x.max())
    y0, y1 = int(y.min()), int(y.max())
    pad = max(32, int(max(x1 - x0, y1 - y0) * padding_fraction))
    return [max(0, x0 - pad), max(0, y0 - pad), min(width - 1, x1 + pad), min(height - 1, y1 + pad)]


def require_cuda(device: str) -> None:
    """Fail early if the requested PyTorch CUDA device is unavailable."""
    if not device.startswith("cuda"):
        return

    try:
        import torch
    except ImportError as exc:
        raise RuntimeError(
            "Install mask dependencies with `uv run --project covoca_toolkit --extra masks ...`."
        ) from exc

    if not torch.cuda.is_available():
        raise RuntimeError(f"Requested {device}, but PyTorch cannot see a CUDA device.")


def load_sam_model(model_path: Path) -> Any:
    """Load MobileSAM, downloading it into `local/models` on first use."""
    try:
        from ultralytics import SAM
    except ImportError as exc:
        raise RuntimeError(
            "Install mask dependencies with `uv run --project covoca_toolkit --extra masks ...`."
        ) from exc

    if model_path.exists():
        return SAM(str(model_path))

    if model_path.name != "mobile_sam.pt":
        raise FileNotFoundError(f"Model file does not exist: {model_path}")

    model_path.parent.mkdir(parents=True, exist_ok=True)
    model = SAM(model_path.name)
    downloaded = Path(model_path.name)
    if downloaded.exists():
        shutil.move(str(downloaded), model_path)
        return SAM(str(model_path))
    return model


def choose_mask(masks: np.ndarray, seed: np.ndarray) -> np.ndarray:
    """Pick the segmentation mask that best covers the prompt seed."""
    masks = masks.astype(bool)
    if masks.ndim == 2:
        masks = masks[np.newaxis, :, :]

    seed_area = max(1, int(seed.sum()))
    scores = [float((mask & seed).sum()) / seed_area for mask in masks]
    return clean_mask(masks[int(np.argmax(scores))])


def sam_mask(image_path: Path, model: Any, device: str, padding_fraction: float) -> np.ndarray:
    """Segment one image with MobileSAM on the requested PyTorch device."""
    rgb = read_rgb(image_path)
    seed = clean_mask(warm_object_seed(rgb))
    box = prompt_box(seed, padding_fraction)
    result = model(str(image_path), bboxes=[box], device=device, verbose=False)[0]
    if result.masks is None:
        raise ValueError(f"SAM returned no masks for {image_path}")

    masks = result.masks.data.detach().cpu().numpy() > 0.5
    return choose_mask(masks, seed)


def make_silhouette_mask(image_path: Path, mask_path: Path, model: Any, device: str, padding_fraction: float) -> None:
    """Write a binary silhouette mask for one image."""
    mask = sam_mask(image_path, model, device, padding_fraction)
    Image.fromarray((mask * 255).astype(np.uint8)).save(mask_path)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate silhouette masks for the cat dataset with MobileSAM.")
    parser.add_argument("--images", type=Path, default=DEFAULT_IMAGES_DIR, help="Directory with source photos")
    parser.add_argument("--masks", type=Path, default=DEFAULT_MASKS_DIR, help="Output directory for masks")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL_PATH, help="MobileSAM weights path")
    parser.add_argument("--device", default=DEFAULT_DEVICE, help="PyTorch device, default: cuda:0")
    parser.add_argument(
        "--prompt-padding",
        type=float,
        default=PROMPT_PADDING_FRACTION,
        help="Padding added around the color-seed prompt box, as a fraction of the box size",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    require_cuda(args.device)
    model = load_sam_model(args.model)
    args.masks.mkdir(parents=True, exist_ok=True)

    image_paths = sorted(args.images.glob("*.jpg"))
    for image_path in image_paths:
        mask_path = args.masks / image_path.with_suffix(".png").name
        make_silhouette_mask(image_path, mask_path, model, args.device, args.prompt_padding)

    print(f"Wrote {len(image_paths)} masks to {args.masks} using {args.model} on {args.device}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
