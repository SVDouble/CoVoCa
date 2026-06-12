# Cat figurine

17 photos of a small wooden cat figurine on the same ArUco GridBoard as
[`aruco_sample`](../aruco_sample/README.md), shot from a ring of angles around
the object on a dark countertop.

Board parameters (same board as `aruco_sample`):

- dictionary: `DICT_6X6_1000`
- markers_x: 4
- markers_y: 5
- marker_length_m: 0.0375
- marker_separation_m: 0.005

## Files

Source photos live in `local/datasets/cat/images/` (`photo_*.jpg`, not
committed).

## Configs

- [`calibration.yaml`](calibration.yaml): `aruco_calibrate calibrate` —
  produces `local/datasets/cat/calibration_result.yaml`.

  ```bash
  ./build/debug/aruco_calibrate calibrate --config datasets/cat/calibration.yaml
  ```

- Masks: generated with MobileSAM from the `ultralytics` package. The script
  uses a conservative color seed only to prompt the segmentation model, then
  writes binary silhouettes into `local/datasets/cat/masks/`. It requires a
  PyTorch CUDA device by default:

  ```bash
  uv run --project covoca_toolkit --extra masks --python 3.14 generate-cat-masks
  ```

- [`voxel_carving.yaml`](voxel_carving.yaml): `voxel_carve run` — carves a
  volume around the figurine and writes colored point cloud/mesh outputs.

  ```bash
  ./build/debug/voxel_carve run --config datasets/cat/voxel_carving.yaml
  ```

- Dashboard:

  ```bash
  uv run --project covoca_toolkit --python 3.14 --extra plot visualize-voxel-carving --config datasets/cat/voxel_carving.yaml
  ```
