#!/usr/bin/env python3.14
# /// script
# requires-python = ">=3.14"
# dependencies = [
#   "numpy==2.4.6",
#   "opencv-python==4.13.0.92",
#   "pillow==12.2.0",
#   "pyyaml==6.0.3",
# ]
# ///
"""Generate camera intrinsics and per-image poses for local datasets.

For each selected dataset under `local/datasets`, this calibrates camera
intrinsics from the available ArUco-board images, then writes:

- `camera_intrinsics.yaml`: the shared calibrated camera profiles for the run.
- `camera/intrinsics.yaml`: the camera profile used by that dataset.
- `camera/poses.yaml`: board-to-camera extrinsics for each usable image.
- `camera/cameras.txt`: flat camera file consumed by the C++ voxel-carving loader.

Outputs go to a new datetime-named folder and never replace existing camera
annotations. Pass `--shared-intrinsics` only when you want to reuse a previous
calibration instead of deriving one from the images.

Usage:
    uv run --script --python 3.14 datasets/generate_camera.py
    uv run --script --python 3.14 datasets/generate_camera.py --dataset cat
    uv run --script --python 3.14 datasets/generate_camera.py --shared-intrinsics local/dataset_metadata/camera_intrinsics.yaml
"""

import argparse
from datetime import datetime
from pathlib import Path
from typing import Any

import numpy as np
import yaml
from PIL import Image

DATASETS_ROOT = Path("local/datasets")
OUTPUT_ROOT = Path("local/annotations/camera")
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png"}
CAMERA_NAME = "pixel7"
CAMERA_MAKE = "Google"
CAMERA_MODEL = "Pixel 7"
CALIBRATION_MIN_MARKERS = 10
CORNER_REFINEMENT_WINDOW = 15
REJECT_FACTOR = 1.5
REJECT_FLOOR_PX = 1.0
MAX_REJECTION_ROUNDS = 6
BOARD = {
    "type": "aruco_grid",
    "dictionary": "DICT_6X6_1000",
    "markers_x": 4,
    "markers_y": 5,
    "marker_length_m": 0.0375,
    "marker_separation_m": 0.005,
}


def load_yaml(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        data = yaml.safe_load(stream)
    if not isinstance(data, dict):
        raise ValueError(f"Expected a YAML mapping: {path}")
    return data


def write_yaml(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        yaml.safe_dump(data, stream, sort_keys=False, width=120)


def image_size(path: Path) -> tuple[int, int] | None:
    try:
        with Image.open(path) as image:
            return image.size
    except OSError:
        return None


def image_paths(dataset: Path) -> list[Path]:
    return sorted(
        path for path in (dataset / "images").iterdir() if path.is_file() and path.suffix.casefold() in IMAGE_EXTENSIONS
    )


def target_sizes(datasets: list[Path]) -> list[tuple[int, int]]:
    sizes = set()
    for dataset in datasets:
        for path in image_paths(dataset):
            size = image_size(path)
            if size:
                width, height = size
                sizes.add((max(width, height), min(width, height)))
    if not sizes:
        raise FileNotFoundError("No readable images found for intrinsic calibration")
    return sorted(sizes, reverse=True)


def aruco_detector(cv2: Any, window: int) -> tuple[Any, Any]:
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_1000)
    board = cv2.aruco.GridBoard(
        (BOARD["markers_x"], BOARD["markers_y"]),
        BOARD["marker_length_m"],
        BOARD["marker_separation_m"],
        dictionary,
    )
    parameters = cv2.aruco.DetectorParameters()
    parameters.cornerRefinementMethod = cv2.aruco.CORNER_REFINE_SUBPIX
    parameters.cornerRefinementWinSize = window
    parameters.cornerRefinementMaxIterations = 60
    parameters.cornerRefinementMinAccuracy = 0.01
    return cv2.aruco.ArucoDetector(dictionary, parameters), board


def collect_calibration_views(
    cv2: Any, datasets: list[Path], target: tuple[int, int], window: int
) -> list[dict[str, Any]]:
    detector, board = aruco_detector(cv2, window)
    portrait = (target[1], target[0])
    views = []
    for dataset in datasets:
        kept = 0
        for path in image_paths(dataset):
            size = image_size(path)
            if size is None or (max(size), min(size)) != target:
                continue
            image = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
            if image is None:
                continue
            if size == portrait:
                image = cv2.rotate(image, cv2.ROTATE_90_CLOCKWISE)
            corners, ids, rejected = detector.detectMarkers(image)
            if ids is not None:
                detector.refineDetectedMarkers(image, board, corners, ids, rejected)
            if ids is None:
                continue
            object_points, points = board.matchImagePoints(corners, ids)
            if object_points is None or len(object_points) < 4:
                continue
            views.append(
                {
                    "dataset": dataset.name,
                    "image": path.name,
                    "markers": int(len(ids)),
                    "object_points": object_points.astype(np.float32),
                    "image_points": points.astype(np.float32),
                }
            )
            kept += 1
        if kept:
            print(f"  {target[0]}x{target[1]} {dataset.name}: {kept} calibration views", flush=True)
    return views


def per_view_rms(
    cv2: Any, views: list[dict[str, Any]], matrix: np.ndarray, dist: np.ndarray, rvecs: list[np.ndarray], tvecs: list[np.ndarray]
) -> np.ndarray:
    errors = []
    for view, rvec, tvec in zip(views, rvecs, tvecs, strict=True):
        projected, _ = cv2.projectPoints(view["object_points"], rvec, tvec, matrix, dist)
        residual = view["image_points"].reshape(-1, 2) - projected.reshape(-1, 2)
        errors.append(float(np.sqrt(np.mean(np.sum(residual * residual, axis=1)))))
    return np.asarray(errors)


def calibrate_intrinsics(
    cv2: Any, views: list[dict[str, Any]], size: tuple[int, int], reject_factor: float, reject_floor: float
) -> dict[str, Any]:
    current = list(views)
    flags = cv2.CALIB_FIX_ASPECT_RATIO | cv2.CALIB_FIX_K3 | cv2.CALIB_FIX_K4 | cv2.CALIB_FIX_K5 | cv2.CALIB_FIX_K6
    rounds = []
    matrix = dist = None
    for _ in range(MAX_REJECTION_ROUNDS):
        object_points = [view["object_points"] for view in current]
        points = [view["image_points"] for view in current]
        rms, matrix, dist, rvecs, tvecs = cv2.calibrateCamera(object_points, points, size, None, None, flags=flags)
        errors = per_view_rms(cv2, current, matrix, dist, rvecs, tvecs)
        threshold = max(reject_floor, float(np.median(errors)) * reject_factor)
        keep = [view for view, error in zip(current, errors, strict=True) if error <= threshold]
        rounds.append(
            {
                "frames": len(current),
                "rms_px": round(float(rms), 4),
                "median_px": round(float(np.median(errors)), 4),
                "max_px": round(float(errors.max()), 4),
                "threshold_px": round(threshold, 4),
                "dropped": len(current) - len(keep),
            }
        )
        print(
            f"  {size[0]}x{size[1]} round {len(rounds)}: "
            f"frames={len(current)} rms={rms:.3f}px drop={len(current) - len(keep)}",
            flush=True,
        )
        if len(keep) == len(current):
            break
        current = keep
    return {"views": current, "matrix": matrix, "dist": dist, "rounds": rounds}


def rounded_matrix(matrix: np.ndarray) -> list[list[float]]:
    return [[round(float(matrix[i, j]), 5) for j in range(3)] for i in range(3)]


def rounded_vector(values: Any) -> list[float]:
    return [round(float(value), 6) for value in np.asarray(values).ravel()]


def portrait_profile(matrix: np.ndarray, dist: np.ndarray, landscape: tuple[int, int]) -> dict[str, Any]:
    width, height = landscape
    fx, fy = float(matrix[0, 0]), float(matrix[1, 1])
    cx, cy = float(matrix[0, 2]), float(matrix[1, 2])
    k1, k2, p1, p2, k3 = (float(value) for value in np.asarray(dist).ravel())
    rotated = np.array([[fy, 0.0, cy], [0.0, fx, (width - 1) - cx], [0.0, 0.0, 1.0]])
    return {
        "width": height,
        "height": width,
        "matrix": rounded_matrix(rotated),
        "distortion_coefficients": rounded_vector([k1, k2, p2, -p1, k3]),
    }


def calibrate_shared_intrinsics(cv2: Any, datasets: list[Path]) -> dict[str, Any]:
    profiles = {}
    methods = []
    for size in target_sizes(datasets):
        views = collect_calibration_views(cv2, datasets, size, CORNER_REFINEMENT_WINDOW)
        views = [view for view in views if view["markers"] >= CALIBRATION_MIN_MARKERS]
        if len(views) < 3:
            print(f"Skipping {size[0]}x{size[1]} intrinsics: only {len(views)} usable calibration views", flush=True)
            continue
        result = calibrate_intrinsics(cv2, views, size, REJECT_FACTOR, REJECT_FLOOR_PX)
        final = result["rounds"][-1]
        landscape_name = f"{CAMERA_NAME}_{size[0]}x{size[1]}"
        portrait_name = f"{CAMERA_NAME}_{size[1]}x{size[0]}"
        profiles[landscape_name] = {
            "width": size[0],
            "height": size[1],
            "matrix": rounded_matrix(result["matrix"]),
            "distortion_coefficients": rounded_vector(result["dist"]),
            "source_session_count": len({view["dataset"] for view in result["views"]}),
            "accepted_frame_count": len(result["views"]),
            "rms_reprojection_error_px": final["rms_px"],
            "median_reprojection_error_px": final["median_px"],
        }
        profiles[portrait_name] = portrait_profile(result["matrix"], result["dist"], size) | {
            "derived_from": landscape_name
        }
        methods.append(f"{size[0]}x{size[1]}: {len(result['views'])} views, RMS {final['rms_px']} px")
    if not profiles:
        raise RuntimeError("No camera intrinsics could be calibrated from the available images")
    return {
        "schema": "covoca.camera_intrinsics.v1",
        "camera": {"make": CAMERA_MAKE, "model": CAMERA_MODEL},
        "method": "ArUco GridBoard calibration from dataset images. " + "; ".join(methods),
        "profiles": profiles,
    }


def rounded(value: np.ndarray) -> list[Any]:
    return np.round(value.astype(np.float64), 9).tolist()


def transform_matrix(rotation: np.ndarray, translation: np.ndarray) -> np.ndarray:
    transform = np.eye(4, dtype=np.float64)
    transform[:3, :3] = rotation
    transform[:3, 3] = translation.reshape(3)
    return transform


def profile_for_size(profiles: dict[str, dict[str, Any]], width: int, height: int) -> tuple[str, dict[str, Any]]:
    matches = [
        (name, profile)
        for name, profile in profiles.items()
        if (profile["width"], profile["height"]) == (width, height)
    ]
    if len(matches) != 1:
        raise ValueError(f"Expected one intrinsics profile for {width}x{height}, found {len(matches)}")
    return matches[0]


def detect_pose(
    cv2: Any,
    detector: Any,
    board: Any,
    image_path: Path,
    profile_name: str,
    profile: dict[str, Any],
    min_markers: int,
    max_error: float,
) -> tuple[dict[str, Any] | None, str | None]:
    image = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
    if image is None:
        return None, "image could not be read"
    height, width = image.shape[:2]
    if (width, height) != (profile["width"], profile["height"]):
        return None, f"image size {width}x{height} does not match {profile_name}"

    corners, ids, rejected = detector.detectMarkers(image)
    if ids is not None:
        detector.refineDetectedMarkers(image, board, corners, ids, rejected)
    marker_count = 0 if ids is None else len(ids)
    if marker_count < min_markers:
        return None, f"only {marker_count} markers detected"

    object_points, image_points = board.matchImagePoints(corners, ids)
    if len(object_points) < 4:
        return None, "fewer than four board corners matched"

    camera_matrix = np.asarray(profile["matrix"], dtype=np.float64)
    distortion = np.asarray(profile["distortion_coefficients"], dtype=np.float64)
    success, raw_rvec, translation = cv2.solvePnP(
        object_points,
        image_points,
        camera_matrix,
        distortion,
        flags=cv2.SOLVEPNP_ITERATIVE,
    )
    if not success:
        return None, "solvePnP failed"

    projected, _ = cv2.projectPoints(object_points, raw_rvec, translation, camera_matrix, distortion)
    residuals = image_points.reshape(-1, 2) - projected.reshape(-1, 2)
    error = float(np.sqrt(np.mean(np.sum(residuals * residuals, axis=1))))
    if error > max_error:
        return None, f"reprojection error {error:.3f} px exceeds {max_error:.3f} px"

    raw_rotation, _ = cv2.Rodrigues(raw_rvec)
    rotation = raw_rotation @ np.diag([1.0, -1.0, -1.0])
    rvec, _ = cv2.Rodrigues(rotation)
    camera_center = -rotation.T @ translation
    return {
        "image": f"images/{image_path.name}",
        "intrinsics_profile": profile_name,
        "detected_marker_count": marker_count,
        "matched_corner_count": len(object_points),
        "mean_reprojection_error_px": round(error, 6),
        "detected_ids": sorted(int(value) for value in ids.reshape(-1)),
        "rvec_board_to_camera": rounded(rvec.reshape(3)),
        "tvec_board_to_camera_m": rounded(translation.reshape(3)),
        "rotation_board_to_camera": rounded(rotation),
        "transform_board_to_camera": rounded(transform_matrix(rotation, translation)),
        "camera_center_board_m": rounded(camera_center.reshape(3)),
    }, None


def pose_document(
    dataset: str,
    board: dict[str, Any],
    frames: list[dict[str, Any]],
    rejected: list[tuple[str, str]],
    *,
    method: str,
) -> dict[str, Any]:
    errors = [frame["mean_reprojection_error_px"] for frame in frames]
    return {
        "schema": "covoca.camera_poses.v1",
        "dataset": dataset,
        "intrinsics_file": "intrinsics.yaml",
        "method": method,
        "coordinate_system": {
            "world_frame": "aruco_gridboard",
            "board_axes": "+X right, +Y up, +Z out of the printed board",
            "camera_axes": "OpenCV: +X right, +Y down, +Z forward",
            "transform_direction": "board_to_camera",
            "translation_unit": "meter",
        },
        "board": board,
        "summary": {
            "input_image_count": len(frames) + len(rejected),
            "accepted_frame_count": len(frames),
            "rejected_frame_count": len(rejected),
            "mean_reprojection_error_px": round(float(np.mean(errors)), 6) if errors else None,
            "max_reprojection_error_px": round(float(np.max(errors)), 6) if errors else None,
        },
        "rejected_frames": [{"image": f"images/{name}", "reason": reason} for name, reason in rejected],
        "frames": frames,
    }


def detect_camera_data(
    cv2: Any,
    dataset: Path,
    shared: dict[str, Any],
    source_label: str,
    min_markers: int,
    max_error: float,
) -> tuple[dict[str, Any], dict[str, Any]]:
    profiles = shared["profiles"]
    dictionary = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_1000)
    board = cv2.aruco.GridBoard(
        (BOARD["markers_x"], BOARD["markers_y"]),
        BOARD["marker_length_m"],
        BOARD["marker_separation_m"],
        dictionary,
    )
    parameters = cv2.aruco.DetectorParameters()
    parameters.cornerRefinementMethod = cv2.aruco.CORNER_REFINE_SUBPIX
    detector = cv2.aruco.ArucoDetector(dictionary, parameters)

    frames = []
    rejected = []
    used_profiles: set[str] = set()
    for image_path in image_paths(dataset):
        image = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        if image is None:
            rejected.append((image_path.name, "image could not be read"))
            continue
        height, width = image.shape
        profile_name, profile = profile_for_size(profiles, width, height)
        frame, reason = detect_pose(cv2, detector, board, image_path, profile_name, profile, min_markers, max_error)
        if frame:
            frames.append(frame)
            used_profiles.add(profile_name)
        else:
            rejected.append((image_path.name, reason or "unknown pose-estimation failure"))

    if not frames:
        raise RuntimeError(f"No camera poses could be estimated for {dataset.name}")
    intrinsics = {
        "schema": shared["schema"],
        "dataset": dataset.name,
        "camera": shared.get("camera"),
        "method": shared["method"],
        "source": source_label,
        "profiles": {name: profiles[name] for name in sorted(used_profiles)},
    }
    return intrinsics, pose_document(
        dataset.name,
        BOARD,
        frames,
        rejected,
        method="ArUco GridBoard detection and solvePnP with fixed shared intrinsics.",
    )


def write_loader_camera_file(path: Path, intrinsics: dict[str, Any], poses: dict[str, Any]) -> None:
    """Write the flat camera format expected by src/DatasetLoader.cpp."""
    frames = poses["frames"]
    with path.open("w", encoding="utf-8") as stream:
        stream.write(f"{len(frames)}\n")
        for frame in frames:
            profile = intrinsics["profiles"][frame["intrinsics_profile"]]
            stream.write(f"{Path(frame['image']).name}\n")
            for row in profile["matrix"]:
                stream.write(" ".join(f"{float(value):.9g}" for value in row) + "\n")
            for row in frame["rotation_board_to_camera"]:
                stream.write(" ".join(f"{float(value):.9g}" for value in row) + "\n")
            stream.write(" ".join(f"{float(value):.9g}" for value in frame["tvec_board_to_camera_m"]) + "\n")


def create_run_dir(output_root: Path) -> Path:
    run_dir = output_root / datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir.mkdir(parents=True, exist_ok=False)
    return run_dir


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--datasets-root", type=Path, default=DATASETS_ROOT)
    parser.add_argument("--output-root", type=Path, default=OUTPUT_ROOT)
    parser.add_argument("--shared-intrinsics", type=Path, help="reuse a camera_intrinsics.yaml from an earlier run")
    parser.add_argument("--dataset", action="append", help="process only this dataset; omit to process all datasets")
    parser.add_argument("--min-markers", type=int, default=4)
    parser.add_argument("--max-reprojection-error", type=float, default=12.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.min_markers < 1 or args.max_reprojection_error <= 0:
        raise ValueError("Marker count and reprojection-error limits must be positive")

    import cv2

    selected = set(args.dataset) if args.dataset else None
    datasets = sorted(
        path
        for path in args.datasets_root.iterdir()
        if path.is_dir() and (path / "images").is_dir() and (selected is None or path.name in selected)
    )
    if not datasets:
        raise FileNotFoundError("No datasets with images/ matched the requested selection")

    run_dir = create_run_dir(args.output_root)
    if args.shared_intrinsics:
        if not args.shared_intrinsics.is_file():
            raise FileNotFoundError(f"Shared intrinsics file does not exist: {args.shared_intrinsics}")
        shared = load_yaml(args.shared_intrinsics)
        source_label = str(args.shared_intrinsics)
    else:
        print("Calibrating shared camera intrinsics from dataset images", flush=True)
        shared = calibrate_shared_intrinsics(cv2, datasets)
        write_yaml(run_dir / "camera_intrinsics.yaml", shared)
        source_label = "../../camera_intrinsics.yaml"

    print(f"Writing camera annotations to {run_dir}", flush=True)
    for dataset in datasets:
        intrinsics, poses = detect_camera_data(
            cv2,
            dataset,
            shared,
            source_label,
            args.min_markers,
            args.max_reprojection_error,
        )

        output = run_dir / dataset.name / "camera"
        write_yaml(output / "intrinsics.yaml", intrinsics)
        write_yaml(output / "poses.yaml", poses)
        write_loader_camera_file(output / "cameras.txt", intrinsics, poses)
        summary = poses["summary"]
        print(
            f"{dataset.name}: {summary['accepted_frame_count']}/{summary['input_image_count']} poses, "
            f"mean={summary['mean_reprojection_error_px']} px, max={summary['max_reprojection_error_px']} px",
            flush=True,
        )
        for rejected in poses["rejected_frames"]:
            print(f"  rejected {Path(rejected['image']).name}: {rejected['reason']}", flush=True)

    print(f"Output: {run_dir}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
