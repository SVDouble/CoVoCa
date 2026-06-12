# Voxel Carving Requirements

Voxel carving is a reference baseline for the Gaussian Splatting work. The MVP
is a deterministic silhouette visual hull from calibrated images and masks.

## References

- Kutulakos and Seitz, "A Theory of Shape by Space Carving", IJCV 2000:
  <https://www.cs.toronto.edu/~kyros/pubs/00.ijcv.carve.pdf>
- Seitz and Dyer, "Photorealistic Scene Reconstruction by Voxel Coloring",
  IJCV 1999: <https://www1.cs.columbia.edu/~allen/PHOTOPAPERS/seitz.long.pdf>
- Voxel benchmark: Open3D voxel carving:
  <https://www.open3d.org/docs/latest/tutorial/Advanced/voxelization.html>
- Gaussian Splatting benchmark: gsplat: <https://docs.gsplat.studio/>
- Paper reference implementation: GraphDeco/Inria gaussian-splatting:
  <https://github.com/graphdeco-inria/gaussian-splatting>

Do not copy implementation code from reference projects.

## Coordinate Convention

- Calibration writes `board_to_camera`: `X_camera = R * X_board + t`.
- The ArUco board frame is the first world frame.
- Voxel carving stores voxels in world coordinates and projects them into each
  camera.

## MVP Pipeline

Inputs:

- `gs.calibration.result.v1` calibration result.
- One binary mask per accepted frame.
- Axis-aligned world-space bounding box.
- Voxel size in meters.

Algorithm:

1. Load calibration, images, and masks.
1. Build a dense voxel volume over the configured AABB.
1. Project each voxel center into each required view.
1. Mark a voxel empty if any required projected sample lands on background.
1. Export surviving voxel centers as PLY.
1. Log input voxel count, occupied count, carved count, and runtime.

MVP exclusions:

- No photo-consistency.
- No visibility ordering.
- No sparse octree.
- No GPU implementation.
- No mesh smoothing.

## Stages

### Stage 1: Silhouette Visual Hull

- Center-sample each voxel.
- Use fixed mask threshold.
- Use configured outside-image policy.
- Export occupied points.
- Compare occupied counts against Open3D.

### Stage 2: Robustness

- Mask threshold in config.
- Optional erosion/dilation.
- Sampling modes: `center`, `corners`, `center_and_corners`.
- Outside-image policies: `carve`, `keep`, `ignore_view`.
- Per-view stats for projected, carved, and skipped voxels.

### Stage 3: Scale

- Rectangular volume dimensions `(nx, ny, nz)`.
- Flat occupancy storage.
- Document one index order.
- Optional parallel voxel loop.
- Optional coarse-to-fine pass.

### Stage 4: Export

- PLY occupied centers.
- PLY cube mesh.
- Metadata sidecar with config path, calibration path, AABB, voxel size, and
  coordinate convention.

### Later

Photo-consistency carving, sparse storage, TSDF/SDF fusion, and GPU carving are
optional after the reference baseline and benchmarks work.

## Config

Add `datasets/aruco_sample/voxel_carving.yaml`:

```yaml
schema: gs.voxel_carving.config.v1
name: sample_visual_hull

paths:
  calibration_result: local/datasets/aruco_sample/calibration_result.yaml
  masks_dir: local/datasets/aruco_sample/masks
  output_dir: local/outputs/aruco_sample

volume:
  min_m: [-0.15, -0.15, 0.0]
  max_m: [0.15, 0.15, 0.3]
  voxel_size_m: 0.005

carving:
  sample_policy: center
  outside_image_policy: keep
  foreground_threshold: 128
  min_required_views: 1

export:
  format: ply
  occupied_points_file: visual_hull_points.ply
  occupied_mesh_file: visual_hull_voxels.ply
```

Use the same reflect-cpp config pattern as calibration:

- one schema string per entrypoint,
- typed structs,
- validators for dimensions, thresholds, and policies,
- no hidden algorithm defaults outside schema constants.

## Required C++ Interfaces

Namespace: `covoca::voxel_carving`.

### `VoxelCarvingConfig`

```cpp
struct VoxelCarvingConfig {
    std::string schema;
    std::string name;
    VoxelCarvingPaths paths;
    VolumeConfig volume;
    CarvingConfig carving;
    ExportConfig exportConfig;
};

VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path& path);
void validateVoxelCarvingConfig(const VoxelCarvingConfig& config);
```

### `SilhouetteMask`

```cpp
class SilhouetteMask {
  public:
    static SilhouetteMask load(const std::filesystem::path& path, int foregroundThreshold);

    bool containsForeground(int x, int y) const;
    bool containsForeground(const Eigen::Vector2d& pixel) const;
    int width() const;
    int height() const;
};
```

### `CalibratedView`

```cpp
struct ProjectionResult {
    bool inFront = false;
    bool insideImage = false;
    Eigen::Vector2d pixel = Eigen::Vector2d::Zero();
    double depthMeters = 0.0;
};

class CalibratedView {
  public:
    CalibratedView(CameraModel camera, FrameCalibrationResult frame, SilhouetteMask mask);

    ProjectionResult projectWorldPoint(const Eigen::Vector3d& pointWorld) const;
    bool isForeground(const Eigen::Vector2d& pixel) const;
    const std::filesystem::path& imagePath() const;
    const std::filesystem::path& maskPath() const;
};
```

### `VoxelVolume`

```cpp
struct GridIndex {
    int x = 0;
    int y = 0;
    int z = 0;
};

class VoxelVolume {
  public:
    static VoxelVolume create(const Eigen::Vector3d& minCorner,
                              const Eigen::Vector3d& maxCorner,
                              double voxelSizeMeters);

    std::size_t voxelCount() const;
    GridIndex gridIndex(std::size_t flatIndex) const;
    std::size_t flatIndex(GridIndex index) const;
    Eigen::Vector3d center(GridIndex index) const;
    std::array<Eigen::Vector3d, 8> corners(GridIndex index) const;
    bool isOccupied(GridIndex index) const;
    void setOccupied(GridIndex index, bool occupied);
};
```

### `SilhouetteConsistency`

```cpp
enum class SamplePolicy { Center, Corners, CenterAndCorners };
enum class OutsideImagePolicy { Carve, Keep, IgnoreView };

class SilhouetteConsistency {
  public:
    explicit SilhouetteConsistency(CarvingConfig config);

    bool isConsistent(const VoxelVolume& volume,
                      GridIndex voxel,
                      std::span<const CalibratedView> views) const;
};
```

### `VoxelCarver`

```cpp
struct CarvingStats {
    std::size_t inputVoxelCount = 0;
    std::size_t occupiedVoxelCount = 0;
    std::size_t carvedVoxelCount = 0;
    double elapsedSeconds = 0.0;
};

struct VoxelCarvingResult {
    VoxelVolume volume;
    CarvingStats stats;
};

class VoxelCarver {
  public:
    explicit VoxelCarver(VoxelCarvingConfig config);

    VoxelCarvingResult run(std::span<const CalibratedView> views) const;
};
```

### `VoxelExport`

```cpp
void writeOccupiedPointsPly(const std::filesystem::path& path, const VoxelVolume& volume);
void writeOccupiedCubeMeshPly(const std::filesystem::path& path, const VoxelVolume& volume);
```

### CLI

```text
voxel_carve run --config datasets/aruco_sample/voxel_carving.yaml
voxel_carve inspect-config --config datasets/aruco_sample/voxel_carving.yaml
voxel_carve benchmark --config datasets/aruco_sample/voxel_carving.yaml
```

## Benchmarks

### Open3D

Use Open3D as the voxel-carving parity benchmark:

- convert calibration result frames to Open3D camera parameters,
- use the same AABB, voxel size, masks, and thresholds,
- run `carve_silhouette`,
- compare occupied count, approximate IoU, and runtime.

Open3D is a parity check, not a correctness oracle. Differences must be tied to
documented policy choices.

### gsplat

Use `gsplat` for the Gaussian Splatting benchmark:

- same calibrated images and train/test split,
- converted camera intrinsics/extrinsics,
- PSNR/SSIM/LPIPS when available,
- training time, render time, GPU memory, and Gaussian count.

## Tests

- Flat index and grid-index round trip.
- Voxel centers for a known AABB.
- Projection of known 3D points.
- Mask threshold behavior.
- Outside-image policy behavior.
- Synthetic masks that carve known voxels.
- PLY header and vertex count.

## Implementation Order

1. Add voxel-carving config.
1. Add `VoxelVolume` and tests.
1. Add `SilhouetteMask` and tests.
1. Add `CalibratedView` projection.
1. Add `SilhouetteConsistency`.
1. Add `VoxelCarver`.
1. Add PLY point export.
1. Add `voxel_carve` CLI.
1. Add Open3D benchmark helper in `covoca_toolkit`.
