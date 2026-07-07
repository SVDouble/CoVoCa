# CoVoCa Voxel Carving

This repository reconstructs object meshes from calibrated images, masks, and
camera poses. The C++ executable does the voxel carving and optional color
reconstruction. Python scripts help with setup, config generation, missing masks
or camera files, and panoramas.

Expected local data layout:

```text
local/datasets/<object>/
  images/
  masks/
  camera/
    intrinsics.yaml
    poses.yaml
```

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Usual Run

Generate one batch config for all objects:

```bash
uv run --script --python 3.14 datasets/create_loader_config.py \
  --batch-config local/configs/all_objects.voxel_carving_batch.yaml \
  --volume-min -0.02 -0.22 0.0 \
  --volume-max 0.2 0.06 0.22 \
  --resolution 120 150 120 \
  --color-methods average best_view weighted_average median \
  --workers 4
```

Run voxel carving:

```bash
./build/main local/configs/all_objects.voxel_carving_batch.yaml
```

The helper uses every object folder under `local/datasets` unless `--object` is
passed. Outputs go to the config's `output_dir`, with one folder per object.
When several color methods are enabled, each method gets its own output folder.

To make a config for only one object:

```bash
uv run --script --python 3.14 datasets/create_loader_config.py \
  --object cat \
  --batch-config local/configs/cat.voxel_carving_batch.yaml \
  --volume-min -0.02 -0.22 0.0 \
  --volume-max 0.2 0.06 0.22 \
  --resolution 120 150 120 \
  --color-methods average best_view weighted_average median

./build/main local/configs/cat.voxel_carving_batch.yaml
```

## Batch Config

A batch config can contain one object or many objects:

```yaml
schema: covoca.branch1.voxel_carving_batch.v1
workers: 4
output_dir: ../results/manual/all_objects
objects:
  - name: cat
    paths:
      images_dir: ../datasets/cat/images
      masks_dir: ../datasets/cat/masks
      camera_dir: ../datasets/cat/camera
    foreground_threshold: 1
    voxel_grid:
      min: [-0.02, -0.22, 0.0]
      max: [0.2, 0.06, 0.22]
      resolution: [120, 150, 120]
    color:
      methods: [average, best_view, weighted_average, median]
```

Edit `voxel_grid.min`, `voxel_grid.max`, and `voxel_grid.resolution` when an
object is clipped, too loose, or too slow to reconstruct. Remove `color` for
geometry-only output.

## Full Setup Helper

If you are starting from images and want the script to prepare missing masks and
camera YAML before reconstruction, use:

```bash
uv run --script --python 3.14 datasets/run_pipeline.py \
  --volume-min -0.02 -0.22 0.0 \
  --volume-max 0.2 0.06 0.22 \
  --resolution 120 150 120 \
  --color-methods average best_view weighted_average median \
  --workers 4
```

Useful options:

```bash
--object <object>          # process only this object
--archive <path>           # install objects from a local archive first
--dataset-url <url>        # download and install objects first
--regenerate-masks         # replace existing masks
--regenerate-camera        # replace existing camera YAML
```

## Quick Checks

Before reconstruction, each object should have:

```bash
ls local/datasets/<object>/images
ls local/datasets/<object>/masks
ls local/datasets/<object>/camera/intrinsics.yaml
ls local/datasets/<object>/camera/poses.yaml
```

## Panoramas

After mesh variants exist:

```bash
uv run --script --python 3.14 datasets/generate_mesh_panoramas.py \
  --mesh-results local/results/<mesh_variants_run> \
  --tile-width 640 \
  --tile-height 480 \
  --combined-scales 0.5 1.0
```
