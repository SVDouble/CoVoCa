"""Download a small public ArUco GridBoard calibration sample."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

DEFAULT_REPO = "abhishekpadalkar/camera_calibration"
DEFAULT_REF = "master"
DEFAULT_OUTPUT = Path("data/sample_aruco")
USER_AGENT = "GaussianSplatting-calibration-data-downloader"


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


def write_text_if_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return
    path.write_text(text, encoding="utf-8")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download MIT-licensed ArUco GridBoard sample images for calibration experiments."
    )
    parser.add_argument("--repo", default=DEFAULT_REPO, help=f"GitHub repo owner/name (default: {DEFAULT_REPO})")
    parser.add_argument("--ref", default=DEFAULT_REF, help=f"Git ref to download (default: {DEFAULT_REF})")
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Output dataset directory (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument("--force", action="store_true", help="Overwrite already-downloaded files")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> int:
    api_root = f"https://api.github.com/repos/{args.repo}/contents"
    raw_root = f"https://raw.githubusercontent.com/{args.repo}/{args.ref}"

    try:
        aruco_data = fetch_json(f"{api_root}/aruco_data?ref={args.ref}")
        if not isinstance(aruco_data, list):
            raise RuntimeError("unexpected GitHub API response for aruco_data")

        image_entries = [
            item
            for item in aruco_data
            if isinstance(item, dict)
            and item.get("type") == "file"
            and str(item.get("name", "")).lower().endswith((".jpg", ".jpeg", ".png"))
        ]
        image_entries.sort(key=lambda item: str(item["name"]))
        if not image_entries:
            raise RuntimeError("no image entries found in remote aruco_data directory")

        downloaded = 0
        for item in image_entries:
            url = str(item["download_url"])
            destination = args.output / "aruco_data" / str(item["name"])
            if download_file(url, destination, args.force):
                downloaded += 1

        for filename in ("LICENSE", "README.md", "aruco_marker_board.pdf"):
            download_file(f"{raw_root}/{filename}", args.output / filename, args.force)

        source_text = f"""# Sample ArUco Calibration Data

Downloaded from <https://github.com/{args.repo}> at ref `{args.ref}`.

The upstream repository describes the data as OpenCV ArUco camera calibration
sample images. The source repository is MIT licensed; see `LICENSE`.

Board defaults used by `aruco_calibrate`:

- dictionary: `DICT_6X6_1000`
- markers_x: 4
- markers_y: 5
- marker_length: 0.0375 m
- marker_separation: 0.005 m
"""
        write_text_if_changed(args.output / "SOURCE.md", source_text)

        dataset_yaml = f"""name: sample_aruco_gridboard
source:
  repository: https://github.com/{args.repo}
  ref: {args.ref}
  license: MIT
images:
  directory: aruco_data
board:
  type: aruco_grid
  dictionary: DICT_6X6_1000
  markers_x: 4
  markers_y: 5
  marker_length_m: 0.0375
  marker_separation_m: 0.005
"""
        write_text_if_changed(args.output / "dataset.yaml", dataset_yaml)

    except (urllib.error.URLError, RuntimeError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"Dataset ready at {args.output}")
    print(f"Images: {len(image_entries)} ({downloaded} downloaded, {len(image_entries) - downloaded} already present)")
    print("Run:")
    print("  ./build/debug/aruco_calibrate calibrate --config configs/calibration/aruco_sample.yaml")
    return 0


def main(argv: list[str] | None = None) -> int:
    return run(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main())
