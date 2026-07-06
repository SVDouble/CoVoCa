# Dataset Inputs

The C++ loader needs three inputs for every usable view:

- original image
- binary foreground mask
- camera intrinsics plus board-to-camera pose

Keep real dataset images under `local/`; do not commit them. Masks and camera
annotations are small enough to commit when they are stable.

## Expected Layout

Raw images:

```text
local/datasets/<dataset>/images/
  image_0001.jpg
  image_0002.jpg
```

Committed masks:

```text
datasets/annotations/<dataset>/masks/
  image_0001.png
  image_0002.png
```

Committed cameras:

```text
datasets/annotations/<dataset>/camera/
  intrinsics.yaml
  poses.yaml
  cameras.txt
```

`cameras.txt` is the file consumed by `src/DatasetLoader.cpp`.

## Fast Path

Masks and camera files are committed for every LRZ object dataset. After
downloading raw images to `local/datasets/<dataset>/images`, build and run:

```bash
cmake -S . -B build
cmake --build build -j
./build/main datasets/annotations/<dataset>/dataset.yaml datasets/annotations/<dataset>/voxel_carving.yaml
```

The `cat` voxel-carving bounds are tested. The other committed
`voxel_carving.yaml` files might need adjustment of the `voxel_grid` values.

## Generate Inputs

For the full automated path:

```bash
uv run --script --python 3.14 datasets/run_pipeline.py \
  --dataset-url "https://syncandshare.lrz.de/getlink/fi5bV88wYMymCG8PHoZTZ5/" \
  --volume-min <x> <y> <z> \
  --volume-max <x> <y> <z>
```

By default this processes every dataset folder in the downloaded archive. Add
`--dataset <dataset>` only when you want one object; repeat it to run a subset.

If `--dataset-url` is omitted, the script prompts for a SyncAndShare link. Press
enter to use datasets already present under `local/datasets`.

The pipeline downloads/extracts the dataset archive, generates masks, generates
camera intrinsics/extrinsics, creates both config files, builds the C++ binary,
then runs voxel carving. Generated run outputs are grouped under:

```text
local/results/<run>/<dataset>/
  camera/
  masks/
  dataset.yaml
  voxel_carving.yaml
  voxel_carving.log
  voxel_grid.ply
  voxel_hull.ply
```

SAM 2.1 weights are stored under `local/models/`, and the Grounding DINO
detector is downloaded by the Python model libraries on first use. If CUDA mask
generation fails, the pipeline retries masks on CPU.

The volume bounds must cover the object in board coordinates. The pipeline
fails if voxel carving produces an empty model.

Example tested on the local `cat` dataset:

```bash
uv run --script --python 3.14 datasets/run_pipeline.py \
  --skip-download \
  --dataset cat \
  --volume-min 0.02 -0.18 0.0 \
  --volume-max 0.15 -0.03 0.15 \
  --resolution 104 120 120
```

For manual debugging, run the individual steps:

```bash
uv run --script --python 3.14 datasets/generate_masks.py --dataset <dataset>
uv run --script --python 3.14 datasets/generate_camera.py --dataset <dataset>
uv run --script --python 3.14 datasets/create_loader_config.py --dataset <dataset>
```

The third command writes two configs. By default it uses committed annotations
from `datasets/annotations/<dataset>` when present. For freshly generated local
annotation runs, pass the generated roots:

```bash
uv run --script --python 3.14 datasets/create_loader_config.py \
  --dataset <dataset> \
  --masks-root local/annotations/segmentation_masks \
  --camera-root local/annotations/camera
```

Pass `--mask-run` or `--camera-run` if you do not want the latest generated run.

Run:

```bash
./build/main local/configs/<dataset>.dataset.yaml local/configs/<dataset>.voxel_carving.yaml
```

The dataset config is only for data locations and mask loading. The voxel
carving config is for reconstruction settings such as volume bounds,
resolution, and color reconstruction.

## Camera File Format

`cameras.txt` is whitespace-separated:

```text
<number_of_views>
<image_filename>
<3x3 intrinsics matrix K>
<3x3 rotation matrix R, board to camera>
<3-vector translation t, board to camera, meters>
```

Repeat the image/K/R/t block once per view. Image filenames are matched by
exact filename first, then by filename stem.
