# Experiment Configurations

Configuration files are grouped by executable or pipeline entry point:

- `calibration/`: camera calibration and coordinate-system visualization.
- `voxel_carving/`: color voxel carving settings.

Paths are interpreted relative to the repository root when using the root
`Makefile` targets.

## Coordinate Frames

Calibration configs currently use OpenCV ArUco GridBoards. The board frame lies
in the marker-board plane. OpenCV documents the GridBoard origin at the bottom
left of the board with `+Z` along the board normal. The camera frame follows
OpenCV convention: `+Z` points forward from the camera into the scene.

Calibration results store `board_to_camera` transforms:

```text
X_camera = R_board_to_camera * X_board + t_board_to_camera
```

For early voxel-carving experiments, the board frame is treated as the world
frame.
