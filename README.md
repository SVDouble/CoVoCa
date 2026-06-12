# Covoca: Color Voxel Carving

Color voxel carving experiments for calibrated multi-view captures.

- Proposal: `docs/proposal/`
- Tooling: `docs/development/tooling.md`
- Voxel carving requirements: `docs/specs/voxel_carving.md`

## Build

System packages:

```bash
sudo apt install cmake g++ make ninja-build clang-format clang-tidy cppcheck valgrind libeigen3-dev libopencv-dev libspdlog-dev libyaml-cpp-dev
```

CMake fetches CLI11 and reflect-cpp if they are not installed. Native vcpkg
users can use `vcpkg.json`.

```bash
cmake --preset debug
cmake --build --preset debug-voxel-carve
./build/debug/voxel_carve run --config datasets/aruco_sample/voxel_carving.yaml
```

Make targets:

```bash
make project
make run
make release
```

## Calibration

Download the sample ArUco dataset:

```bash
uv run --project covoca_toolkit --python 3.14 download-aruco-sample
```

Run calibration:

```bash
cmake --preset debug
cmake --build --preset debug-aruco-calibrate
./build/debug/aruco_calibrate calibrate --config datasets/aruco_sample/calibration.yaml
```

Result:

```text
local/datasets/aruco_sample/calibration_result.yaml
```

Draw coordinate axes:

```bash
./build/debug/aruco_calibrate visualize --result local/datasets/aruco_sample/calibration_result.yaml --output-dir local/datasets/aruco_sample/calibration_axes --axis-length-m 0.05625
```

Board `X`/`Y` lie in the board plane; board `+Z` is the board normal, pointing
out of the printed board toward the camera-facing side (OpenCV's raw
`GridBoard` frame has `+Z` pointing into the board, so calibration flips its
`Y` and `Z` axes to get this convention). Camera `+Z` points forward along the
optical axis. Results store `board_to_camera`, mapping board/world points into
camera coordinates.

For your own capture, copy `datasets/aruco_sample/calibration.yaml` and edit
paths plus board geometry.

Make targets:

```bash
make calibration
make download-sample-data
make calibrate-sample
make visualize-calibration
```

## Quality

```bash
make format-check
make lint
make static-analysis
make python-check
make sanitize
make memcheck
make pre-commit
```

Install hooks:

```bash
make pre-commit-install
```

Build proposal PDF:

```bash
make docs
```

## Layout

```text
apps/                 C++ executables
datasets/             dataset descriptions + entry-point YAML configs
docs/development/     tooling notes
docs/proposal/        LaTeX proposal
docs/specs/           implementation requirements
covoca_toolkit/       uv-managed Python helpers
include/covoca/       public C++ headers
src/covoca/           C++ implementations
local/                downloaded datasets + pipeline outputs (gitignored)
```

## Datasets

See [`datasets/README.md`](datasets/README.md). Each dataset has a
`uv run --project covoca_toolkit` download command and one or more configs
under `datasets/<name>/`; downloaded files and pipeline outputs go to
`local/datasets/<name>/` and `local/outputs/<name>/`.

## Pipeline

1. Capture calibrated multi-view images.
1. Estimate intrinsics and poses with OpenCV ArUco boards.
1. Create foreground masks.
1. Build an Open3D-comparable voxel-carving baseline.
1. Apply color voxel carving for textured reconstructions.

## Dependencies

- OpenCV: image I/O, ArUco, calibration, masks.
- Eigen: linear algebra.
- spdlog: C++ logging.
- CLI11: command-line parsing.
- reflect-cpp + yaml-cpp: typed YAML/JSON config and result IO.
- PyTorch + Ultralytics: optional CUDA segmentation for sample masks.
- CUDA/OpenGL: optional later acceleration and visualization.
