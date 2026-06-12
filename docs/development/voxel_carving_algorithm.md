# Voxel Carving Algorithm

This document explains how `covoca`'s color voxel carving pipeline works,
step by step, for readers with no prior computer-vision background. It
follows the code in `include/covoca/voxel_carving/` and
`src/covoca/voxel_carving/` in execution order.

For requirements and interface signatures, see
[`../specs/voxel_carving.md`](../specs/voxel_carving.md). This document
focuses on *how* the algorithm computes its result.

## 1. Problem statement

Given:

- a set of calibrated camera views, each with a binary silhouette mask
  (foreground/background per pixel), and
- an axis-aligned 3D bounding box subdivided into a grid of cubic voxels,

determine, for each voxel, whether it is **occupied** (consistent with being
part of the object in every view) or **carved** (provably outside the object,
because at least one view's silhouette marks its projection as background).

The set of occupied voxels after processing the whole grid is the **visual
hull**: the largest 3D shape consistent with all input silhouettes.

## 2. Inputs

All inputs are described by a single config file (see
`datasets/aruco_sample/voxel_carving.yaml`).

### 2.1 Voxel-carving config (`VoxelCarvingConfig`)

Loaded and validated by `loadVoxelCarvingConfig()` /
`validateVoxelCarvingConfig()` (`VoxelCarvingConfig.cpp`). This is a
utilitarian step: parse YAML/JSON into a typed struct, then check invariants
the type system cannot express:

- `paths.calibration_result`, `paths.masks_dir`, `paths.output_dir`,
  `export.occupied_points_file`, `export.occupied_mesh_file`: non-empty.
- `volume.max_m > volume.min_m` on every axis.
- `carving.foreground_threshold` in `[0, 255]`.

The values that drive the algorithm are:

- `volume.min_m`, `volume.max_m`: axis-aligned bounding box, in meters, in
  the calibration board's world frame.
- `volume.voxel_size_m`: edge length of each cubic voxel, in meters.
- `carving.sample_policy`: `"center"`, `"corners"`, or
  `"center_and_corners"` — which points of each voxel to test.
- `carving.outside_image_policy`: `"carve"`, `"keep"`, or `"ignore_view"` —
  how to treat a sample that projects outside a view's image.
- `carving.foreground_threshold`: grayscale threshold in `[0, 255]` used to
  binarize each mask.

### 2.2 Calibration result

Loaded via `covoca::calibration::loadCalibrationResult()`. Also a utilitarian
input-loading step. For each accepted frame it provides:

- `camera.matrix`: 3x3 intrinsic matrix (`fx`, `fy`, `cx`, `cy`).
- `rotation_board_to_camera`, `tvec_board_to_camera_m`: extrinsics mapping a
  point from world (board) coordinates to this camera's coordinate system,
  via `X_camera = R * X_world + t`.
- `image`: path to the frame's source image, used to locate the matching
  mask file (`masks_dir / <image stem>.png`).

### 2.3 Silhouette masks

One grayscale image per accepted frame, in `paths.masks_dir`. Loaded by
`SilhouetteMask::load()`, which thresholds the image at
`carving.foreground_threshold`: pixels >= threshold become foreground (255),
the rest become background (0).

## 3. Core algorithm

### 3.1 Build the voxel grid

`VoxelVolume::create(min_m, max_m, voxel_size_m)`:

- Grid dimensions: `(nx, ny, nz) = ceil((max_m - min_m) / voxel_size_m)`,
  clamped to a minimum of 1 per axis.
- All `nx * ny * nz` voxels start **occupied**.
- Voxels are stored as a flat `uint8_t` array, indexed by
  `flatIndex = (z * ny + y) * nx + x`.
- `center(index)` and `corners(index)` map a grid coordinate `(x, y, z)` to
  world-space points (in meters): `center = min_m + (index + 0.5) * voxel_size_m`; the 8 corners are `min_m + (index + {0,1}) * voxel_size_m`.

### 3.2 Project a 3D point into a view

`CalibratedView::projectWorldPoint(P_world)` computes the pixel a 3D point
maps to in one camera, given the camera's extrinsics `(R, t)` and intrinsics
`(fx, fy, cx, cy)`:

1. **World to camera space**: `P_camera = R * P_world + t`.
1. **Front-of-camera check**: if `P_camera.z <= 0`, the point is behind the
   camera; return `inFront = false` and stop.
1. **Pinhole projection**:
   ```
   pixel_x = fx * (P_camera.x / P_camera.z) + cx
   pixel_y = fy * (P_camera.y / P_camera.z) + cy
   ```
1. **Image-bounds check**: `insideImage = (0 <= pixel_x < width) and (0 <= pixel_y < height)`.

Output: `ProjectionResult{inFront, insideImage, pixel, depthMeters}`.

> This implementation uses the pinhole formula only — it does not apply lens
> distortion correction, even though the calibration result may include
> distortion coefficients. This is a deliberate Stage-1 simplification and a
> possible source of small systematic error near image edges.

### 3.3 Test one voxel against one view

Given a voxel's sample points `S = {s_1, ..., s_k}` (from
`carving.sample_policy`) and a view `V`:

For each `s_i`:

- Project `s_i` with `V.projectWorldPoint(s_i)`.
- If `!inFront || !insideImage`, apply `carving.outside_image_policy`:
  - `"carve"`: treat `s_i` as background for `V`.
  - `"keep"`: ignore `s_i`, continue with the remaining samples.
  - `"ignore_view"`: discard `V` entirely for this voxel; no sample in `V`
    can affect the result.
- Otherwise, look up `V.isForeground(pixel)`. If the pixel is background,
  treat `s_i` as background for `V`.

Result for `(voxel, V)`: `V` votes "background" if any non-ignored sample was
treated as background; otherwise `V` votes "foreground".

### 3.4 Combine votes across views (`SilhouetteConsistency::isConsistent`)

A voxel is **consistent** (kept occupied) if and only if every non-ignored
view votes "foreground". If any view votes "background", the voxel is
**inconsistent** and must be carved.

### 3.5 Carve the grid (`VoxelCarver::run`)

```text
for flat in 0 .. voxelCount - 1:
    voxel = gridIndex(flat)
    if not isConsistent(volume, voxel, views):
        setOccupied(voxel, false)
        carvedVoxelCount += 1
    else:
        occupiedVoxelCount += 1
```

Output: `VoxelCarvingResult{volume, stats}`, where `stats` records
`inputVoxelCount` (= `voxelCount()`), `occupiedVoxelCount`,
`carvedVoxelCount`, and `elapsedSeconds`.

### 3.6 Export

`VoxelExport.cpp` writes the carved volume in ASCII PLY or ASCII OFF,
selected by `export.format`:

- `writeOccupiedPoints`: one vertex per occupied voxel, at `center(index)`.
  Under `"off"`, the file has zero faces.
- `writeOccupiedCubeMesh`: per occupied voxel, the 8 `corners(index)` as
  vertices plus 12 triangles (2 per face, via a fixed face table indexed by
  the `dz * 4 + dy * 2 + dx` corner ordering).

Both files are written to `paths.output_dir`, named by `export.*`.

## 4. Color reconstruction (optional)

If the config has a `color` section, `voxel_carve` additionally assigns an
RGB color to every occupied voxel and writes a second, colored pair of
point-cloud/mesh files named by `color.occupied_points_file` and
`color.occupied_mesh_file`.

### 4.1 Loading color images

When `color` is present, each `CalibratedView` also loads a `ColorImage` from
the frame's source image (`frame.image` — the same image used for
calibration and silhouette extraction). `ColorImage::colorAt(pixel)` returns
the `(red, green, blue)` value at a pixel, or `std::nullopt` if the pixel is
outside the image.

### 4.2 Per-voxel sample gathering (`VoxelColorizer::run`)

For each occupied voxel:

1. Project the voxel's `center(index)` into every view
   (`CalibratedView::projectWorldPoint`).
1. Keep only views where the projection is in front of the camera, inside
   the image, and `isForeground(pixel)` is true.
1. Sample that view's color image at the projected pixel. Skip views with no
   loaded color image or whose pixel falls outside it.
1. Compute a per-view weight `max(0, normal . view_direction)`, where
   `view_direction` points from the voxel center toward
   `CalibratedView::cameraOrigin()`, and `normal` is the voxel's estimated
   outward surface normal (§4.3). If no normal could be estimated, every
   view gets weight `1`.

Each kept view contributes one `ColorSample{rgb, weight}`. Voxels with zero
contributing samples keep the default black color `(0, 0, 0)`.

### 4.3 Surface normal estimation

`estimateOutwardNormal` (in `VoxelColorizer.cpp`) estimates a voxel's outward
normal from the occupancy gradient of its 6-neighborhood: for each axis, it
subtracts the occupancy of the `-1` neighbor from the occupancy of the `+1`
neighbor (neighbors outside the grid count as unoccupied). Occupancy
increases toward the interior, so the outward normal is `-gradient`,
normalized. If the gradient is zero (e.g. a fully interior voxel with no
exposed face), the normal is the zero vector and every view gets weight `1`.

### 4.4 Combining samples (`ColorReconstruction.h`)

The collected `ColorSample`s are combined into one `(red, green, blue)` color
by the `ColorReconstructor` selected by `color.method`
(`ColorReconstruction.cpp`):

- **`"average"`** (Method 1, Color Averaging): mean RGB over all samples,
  ignoring weights. Reference: Seitz and Dyer, "Photorealistic Scene
  Reconstruction by Voxel Coloring", IJCV 1999
  (<https://www1.cs.columbia.edu/~allen/PHOTOPAPERS/seitz.long.pdf>).
- **`"best_view"`** (Method 2, Best Viewpoint Selection): the color from the
  sample with the highest weight, i.e. the view whose viewing direction is
  most aligned with the voxel's normal. Reference: Debevec, Borshukov, and Yu,
  "Efficient View-Dependent Image-Based Rendering with Projective
  Texture-Mapping", EGRW 1998
  (<https://www.pauldebevec.com/Research/VDTM/debevec_vdtm_egrw98.pdf>).
- **`"weighted_average"`** (Method 3, Weighted Color Averaging): RGB averaged
  with each sample weighted by its `weight`; falls back to `"average"` if
  every weight is zero. Reference: Buehler, Bosse, McMillan, Gortler, and
  Cohen, "Unstructured Lumigraph Rendering", SIGGRAPH 2001.
- **`"median"`** (Method 4, Median Color Selection): per-channel median over
  all samples, ignoring weights. Robust to outlier observations (occlusion
  errors, specular highlights, calibration drift) compared to averaging.

Each method is a free function; `voxel_carve` selects the configured one via
`colorReconstructorFor(color.method)`, which returns a `ColorReconstructor`
function pointer reused for every voxel.

### 4.5 Colored export

`VoxelColorizer::run` returns a `VoxelColors` buffer (one `(red, green, blue)` triple per voxel, flat-indexed like `VoxelVolume`). `VoxelExport`'s
`writeOccupiedPoints`/`writeOccupiedCubeMesh` take an optional
`const VoxelColors*`:

- PLY: adds `property uchar red`/`green`/`blue` after the `x`/`y`/`z`
  properties, and writes three extra integer columns per vertex.
- OFF: the header becomes `COFF` instead of `OFF`, with the same extra
  columns.

For the cube mesh, all 8 corner vertices of a voxel share that voxel's color.

## 5. Worked example

With `volume.min_m = [-0.15, -0.15, 0.0]`, `volume.max_m = [0.15, 0.15, 0.3]`,
`voxel_size_m = 0.005`: grid dimensions `(60, 60, 60)`, `voxelCount = 216000`.

For voxel `index = (30, 30, 10)`:

- `center(index) = (-0.15 + 30.5*0.005, -0.15 + 30.5*0.005, 0 + 10.5*0.005)`
  `≈ (0.0025, 0.0025, 0.0525)` meters.
- For each view, transform this point with `(R, t)`, project with the
  pinhole formula, and check the resulting pixel against that view's mask.
- If every view votes "foreground" (directly, or via `"keep"`/
  `"ignore_view"`), the voxel remains occupied; otherwise it is carved.

## 6. Glossary

- **Voxel**: a cubic cell of 3D space; the 3D analogue of a pixel.
- **Visual hull**: the largest 3D shape consistent with a set of silhouette
  images.
- **Silhouette mask**: a binary image marking subject (foreground) vs.
  background per pixel.
- **Intrinsics**: parameters mapping 3D camera-space points to 2D pixels
  (`fx`, `fy`, `cx`, `cy`).
- **Extrinsics**: rotation `R` and translation `t` mapping world coordinates
  to camera coordinates.
- **Pinhole camera model**: the projection formula in §3.2; excludes lens
  distortion.
- **PLY**: a text/binary file format for point clouds and meshes.
- **OFF**: a plain-text mesh file format (`OFF\n<#vertices> <#faces> <#edges>`,
  followed by vertex coordinates and face vertex-index lists).
