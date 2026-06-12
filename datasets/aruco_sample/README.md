# ArUco GridBoard sample

OpenCV ArUco GridBoard images for camera calibration, downloaded from
<https://github.com/abhishekpadalkar/camera_calibration> (MIT licensed).

Board parameters:

- dictionary: `DICT_6X6_1000`
- markers_x: 4
- markers_y: 5
- marker_length_m: 0.0375
- marker_separation_m: 0.005

## Download

```bash
uv run --project covoca_toolkit --python 3.14 download-aruco-sample
```

Writes images to `local/datasets/aruco_sample/aruco_data/`.

## Configs

- [`calibration.yaml`](calibration.yaml): `aruco_calibrate calibrate` —
  produces `local/datasets/aruco_sample/calibration_result.yaml`.
- [`voxel_carving.yaml`](voxel_carving.yaml): `voxel_carve run` — requires
  silhouette masks in `local/datasets/aruco_sample/masks/` (not provided by
  the download).
