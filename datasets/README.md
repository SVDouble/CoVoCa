# Datasets

Each subdirectory describes one dataset used by the C++ pipelines:

- `README.md`: what the dataset is, where it comes from, and its license.
- `*.yaml`: entry-point configs (`calibration.yaml`, `voxel_carving.yaml`, ...)
  that reference the dataset's files under `local/datasets/<name>/`.

Dataset *files themselves* (images, masks, calibration results, ...) are not
committed. Each dataset provides a `uv run --project covoca_toolkit` command
(see its `README.md`) that downloads and/or generates them into
`local/datasets/<name>/`.

Pipeline outputs (point clouds, meshes, dashboards) are written to
`local/outputs/<name>/`. `local/` is gitignored in its entirety.

## Available datasets

- [`aruco_sample/`](aruco_sample/README.md): ArUco GridBoard images for
  camera calibration.
- [`dino_sparse_ring/`](dino_sparse_ring/README.md): Middlebury
  "dinoSparseRing" multi-view dataset, for voxel carving with known
  ground-truth camera poses.
