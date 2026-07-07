# Dataset Onboarding

Keep downloaded data under `local/datasets` and do not commit images, masks,
camera files, meshes, or panoramas.

Each object must have:

```text
local/datasets/<object>/
  images/
  masks/
  camera/
    intrinsics.yaml
    poses.yaml
```

The LRZ archive should already include all three folders. The C++ loader reads
camera YAML directly.

## Build

Use a C++23-capable compiler.

```bash
cmake -S . -B build
cmake --build build -j
```

## Generate Starter Configs

Use the Python helper once per object to create editable YAML configs:

```bash
uv run --script --python 3.14 datasets/create_loader_config.py \
  --dataset <object> \
  --volume-min <x> <y> <z> \
  --volume-max <x> <y> <z> \
  --resolution <nx> <ny> <nz> \
  --color-methods average best_view weighted_average median
```

This writes:

```text
local/configs/<object>.dataset.yaml
local/configs/<object>.voxel_carving.yaml
```

## Edit The Configs

The dataset config should usually not need manual edits:

```yaml
schema: covoca.branch1.dataset.v1
name: <object>

paths:
  images_dir: local/datasets/<object>/images
  masks_dir: local/datasets/<object>/masks
  camera_dir: local/datasets/<object>/camera
foreground_threshold: 1
```

Edit the voxel-carving config to tune reconstruction quality:

```yaml
schema: covoca.branch1.voxel_carving.v1
name: <object>

voxel_grid:
  min: [<x>, <y>, <z>]
  max: [<x>, <y>, <z>]
  resolution: [<nx>, <ny>, <nz>]

color:
  methods: [average, best_view, weighted_average, median]
```

Use bounds that contain the object in board coordinates. Higher resolution gives
more detail but increases runtime. Remove the `color` section to skip color
reconstruction.

## Run Voxel Carving

For one object, run the C++ executable from the directory where outputs should
be written:

```bash
ROOT=$PWD
OBJECT=<object>
RESULT=local/results/manual/$OBJECT
mkdir -p "$RESULT"

(
  cd "$RESULT"
  "$ROOT/build/main" \
    "$ROOT/local/configs/$OBJECT.dataset.yaml" \
    "$ROOT/local/configs/$OBJECT.voxel_carving.yaml"
)
```

With one color method, `voxel_grid.ply` and `voxel_hull.ply` are written into
the result directory. With multiple methods, each method gets its own subfolder.

To carve multiple objects in one run, create a batch config:

```yaml
schema: covoca.branch1.voxel_carving_batch.v1
output_dir: ../results/manual

objects:
  - name: cat
    dataset_config: cat.dataset.yaml
    voxel_grid:
      min: [<x>, <y>, <z>]
      max: [<x>, <y>, <z>]
      resolution: [<nx>, <ny>, <nz>]
    color:
      methods: [average, best_view, weighted_average, median]
```

Paths in the batch config are relative to the batch config file. `output_dir`
is shared; each object writes to `output_dir/<name>`. Add an object-level
`output_dir` only when one object needs a different target directory. Objects
in a batch run in parallel.

```bash
./build/main local/configs/voxel_carving_batch.yaml
```

## Optional Setup Helper

`datasets/run_pipeline.py` is still useful for initial setup or batch runs. It
uses existing `local/datasets` by default and can generate missing masks or
camera YAML from images:

```bash
uv run --script --python 3.14 datasets/run_pipeline.py \
  --volume-min <x> <y> <z> \
  --volume-max <x> <y> <z> \
  --resolution <nx> <ny> <nz> \
  --color-methods average best_view weighted_average median
```

Pass `--dataset <object>` to limit it to one object. Downloading is opt-in via
`--dataset-url` or `--archive`.

## Panoramas

After mesh variants exist, create comparison panoramas with:

```bash
uv run --script --python 3.14 datasets/generate_mesh_panoramas.py \
  --mesh-results local/results/<mesh_variants_run> \
  --tile-width 640 \
  --tile-height 480 \
  --combined-scales 0.5 1.0
```

Labels scale with tile size and use a larger default label scale. Increase
`--label-scale` further if labels still need to be larger.
