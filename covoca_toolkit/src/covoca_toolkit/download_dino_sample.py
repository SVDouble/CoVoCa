"""Download and prepare the Middlebury "dinoSparseRing" voxel-carving sample.

Downloads the dataset's images and camera-parameter file from the
maximm8/VisualHull mirror, then derives silhouette masks and a
`gs.calibration.result.v1` calibration result so `voxel_carve` can run
against it directly.
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

import numpy as np
import yaml
from PIL import Image, ImageFilter

DEFAULT_REPO = "maximm8/VisualHull"
DEFAULT_REF = "master"
DEFAULT_SOURCE_DIR = "dinoSparseRing"
DEFAULT_OUTPUT = Path("local/datasets/dino_sparse_ring")
USER_AGENT = "Covoca-dataset-downloader"

# Per the dataset's README.txt silhouette-extraction recipe: threshold at
# 0.19 of [0, 1], dilate by 10px, erode by 7px.
SILHOUETTE_THRESHOLD = round(0.19 * 255)


def request_url(url: str) -> urllib.request.Request:
    return urllib.request.Request(url, headers={"User-Agent": USER_AGENT})


def fetch_json(url: str) -> object:
    with urllib.request.urlopen(request_url(url), timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def download_file(url: str, destination: Path, force: bool) -> bool:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and not force:
        return False

    with urllib.request.urlopen(request_url(url), timeout=60) as response:
        destination.write_bytes(response.read())
    return True


def make_silhouette_mask(image_path: Path, mask_path: Path) -> None:
    gray = Image.open(image_path).convert("L")
    binary = gray.point(lambda p: 255 if p > SILHOUETTE_THRESHOLD else 0)
    binary = binary.filter(ImageFilter.MaxFilter(21))  # dilate ~10px
    binary = binary.filter(ImageFilter.MinFilter(15))  # erode ~7px
    binary.save(mask_path)


def rotation_to_rvec(rotation: np.ndarray) -> np.ndarray:
    """Rotation matrix -> Rodrigues vector (inverse Rodrigues formula)."""
    angle = np.arccos(np.clip((np.trace(rotation) - 1) / 2, -1.0, 1.0))
    if np.isclose(angle, 0.0):
        return np.zeros(3)
    axis = np.array(
        [
            rotation[2, 1] - rotation[1, 2],
            rotation[0, 2] - rotation[2, 0],
            rotation[1, 0] - rotation[0, 1],
        ]
    )
    return axis / (2 * np.sin(angle)) * angle


def write_calibration_result(output: Path, images_dir: Path, par_lines: list[str], good_images: set[str]) -> None:
    num_images = int(par_lines[0])
    frames = []
    camera_matrix = None
    width = height = 0

    for line in par_lines[1 : num_images + 1]:
        parts = line.split()
        name = parts[0]
        if name not in good_images:
            continue

        values = [float(v) for v in parts[1:]]
        k = np.array(values[0:9]).reshape(3, 3)
        r = np.array(values[9:18]).reshape(3, 3)
        t = np.array(values[18:21])

        if camera_matrix is None:
            camera_matrix = k
            with Image.open(images_dir / name) as im:
                width, height = im.size

        transform = np.eye(4)
        transform[:3, :3] = r
        transform[:3, 3] = t

        frames.append(
            {
                "image": (images_dir / name).as_posix(),
                "detected_marker_count": 1,
                "matched_corner_count": 1,
                "mean_reprojection_error_px": 0.0,
                "detected_ids": [0],
                "rvec_board_to_camera": rotation_to_rvec(r).tolist(),
                "tvec_board_to_camera_m": t.tolist(),
                "rotation_board_to_camera": r.tolist(),
                "transform_board_to_camera": transform.tolist(),
                "camera_center_board_m": (-r.T @ t).tolist(),
            }
        )

    result = {
        "schema": "gs.calibration.result.v1",
        "name": "dino_sparse_ring",
        "source": {
            "config": "datasets/dino_sparse_ring/README.md",
            "image_path": images_dir.as_posix(),
            "input_image_count": num_images,
            "accepted_frame_count": len(frames),
        },
        "image_size": {"width": width, "height": height},
        "rms_reprojection_error_px": 0.0,
        "board": {
            "type": "aruco_grid",
            "dictionary": "DICT_6X6_1000",
            "markers_x": 1,
            "markers_y": 1,
            "marker_length_m": 1.0,
            "marker_separation_m": 0.0,
        },
        "camera": {
            "matrix": camera_matrix.tolist(),
            "distortion_coefficients": [0.0, 0.0, 0.0, 0.0, 0.0],
        },
        "frames": frames,
    }

    with open(output, "w") as f:
        yaml.dump(result, f, sort_keys=False, default_flow_style=None)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download the Middlebury dinoSparseRing voxel-carving sample and derive masks/calibration."
    )
    parser.add_argument("--repo", default=DEFAULT_REPO, help=f"GitHub repo owner/name (default: {DEFAULT_REPO})")
    parser.add_argument("--ref", default=DEFAULT_REF, help=f"Git ref to download (default: {DEFAULT_REF})")
    parser.add_argument(
        "--source-dir",
        default=DEFAULT_SOURCE_DIR,
        help=f"Dataset directory in the repo (default: {DEFAULT_SOURCE_DIR})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output dataset directory (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument("--force", action="store_true", help="Re-download and regenerate already-present files")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> int:
    api_root = f"https://api.github.com/repos/{args.repo}/contents/{args.source_dir}"
    raw_root = f"https://raw.githubusercontent.com/{args.repo}/{args.ref}/{args.source_dir}"
    images_dir = args.output / "images"
    masks_dir = args.output / "masks"

    try:
        entries = fetch_json(f"{api_root}?ref={args.ref}")
        if not isinstance(entries, list):
            raise RuntimeError("unexpected GitHub API response for dinoSparseRing contents")

        image_names = sorted(
            str(item["name"])
            for item in entries
            if isinstance(item, dict) and item.get("type") == "file" and str(item.get("name", "")).endswith(".png")
        )
        if not image_names:
            raise RuntimeError("no .png entries found in remote dinoSparseRing directory")

        downloaded = 0
        for name in image_names:
            if download_file(f"{raw_root}/{name}", images_dir / name, args.force):
                downloaded += 1

        for filename in ("dinoSR_par.txt", "dinoSR_good_silhouette_images.txt", "README.txt"):
            download_file(f"{raw_root}/{filename}", args.output / filename, args.force)

        good_images = {
            line.strip() for line in (args.output / "dinoSR_good_silhouette_images.txt").read_text().splitlines() if line.strip()
        }

        masks_dir.mkdir(parents=True, exist_ok=True)
        for name in good_images:
            mask_path = masks_dir / Path(name).with_suffix(".png").name
            if args.force or not mask_path.exists():
                make_silhouette_mask(images_dir / name, mask_path)

        par_lines = (args.output / "dinoSR_par.txt").read_text().splitlines()
        write_calibration_result(args.output / "calibration_result.yaml", images_dir, par_lines, good_images)

    except (urllib.error.URLError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"Dataset ready at {args.output}")
    print(f"Images: {len(image_names)} ({downloaded} downloaded), masks: {len(good_images)}")
    print("Run:")
    print("  ./build/debug/voxel_carve run --config datasets/dino_sparse_ring/voxel_carving.yaml")
    return 0


def main(argv: list[str] | None = None) -> int:
    return run(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
