# Gaussian Splatting Reconstruction

This project compares voxel carving and 3D Gaussian Splatting for reconstructing objects from calibrated multi-view image captures.

The broader project plan is documented in `docs/proposal/`.

## Current Status

- Build system and editor setup are in place: CMake presets, a root Makefile, clangd config, VS Code tasks, and a minimal devcontainer.
- The code currently contains basic `Voxel` and `VoxelGrid` classes plus a small prototype executable.
- The proposal can be built locally from `docs/proposal`.
- Next implementation work is camera calibration/data loading and silhouette-based voxel carving.

## Build And Run

Install the current required dependencies:

```bash
sudo apt install cmake g++ make ninja-build libeigen3-dev
```

For the proposal PDF, install a LaTeX distribution with `pdflatex` and `bibtex`, for example:

```bash
sudo apt install texlive-latex-extra texlive-fonts-recommended
```

The presets require CMake 3.20 or newer.

Configure and build with the default debug preset:

```bash
cmake --preset debug
cmake --build --preset debug-main
```

Or use the root Makefile:

```bash
make project
```

Run the prototype:

```bash
./build/debug/main
```

For an optimized build, use `release` and `release-main` instead.

Build the proposal PDF:

```bash
make docs
```

The PDF is written to `build/docs/proposal/main.pdf`.

## VS Code

The repository includes CMake presets and shared VS Code workspace settings. With the recommended CMake Tools extension installed, VS Code will pick up the `debug` and `release` configure presets automatically.

Available tasks:

- `Make: project`
- `Make: release`
- `Make: run`
- `Make: docs`

## Project Structure

```text
.
├── CMakeLists.txt        # Build configuration
├── CMakePresets.json     # Debug/release configure and build presets
├── Makefile              # Convenience targets for code and docs builds
├── .clangd               # clangd compile database configuration
├── .devcontainer/        # Minimal C++ development container
├── .vscode/              # Shared VS Code tasks and extension recommendations
├── docs/proposal/        # LaTeX proposal, references, and milestones
├── include/              # Header-only project code for now
└── src/                  # Executable entry points
```

## Intended Pipeline

1. Capture object images from multiple viewpoints.
2. Estimate camera intrinsics and extrinsics with OpenCV and ArUco marker boards.
3. Generate foreground masks for each calibrated view.
4. Reconstruct a voxel-carving baseline from silhouette consistency.
5. Initialize and optimize a simplified Gaussian scene representation.
6. Compare both methods by visual quality, runtime, memory use, and robustness.

## Expected Future Dependencies

- OpenCV: image I/O, ArUco detection, calibration, masks, and image processing.
- nlohmann/json or yaml-cpp: dataset and experiment configuration.
- CUDA: optional acceleration for splatting and optimization.
- OpenGL: optional interactive visualization.
