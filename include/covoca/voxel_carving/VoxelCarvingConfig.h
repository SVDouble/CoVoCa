#pragma once

#include <filesystem>
#include <string>

#include <Eigen/Dense>
#include <rfl/Literal.hpp>
#include <rfl/Rename.hpp>

#include "covoca/config/EigenReflectors.h"
#include "covoca/config/Validators.h"

namespace covoca::voxel_carving {

using ConfigSchema = rfl::Literal<"gs.voxel_carving.config.v1">;
using SamplePolicy = rfl::Literal<"center", "corners", "center_and_corners">;
using OutsideImagePolicy = rfl::Literal<"carve", "keep", "ignore_view">;
using covoca::config::PositiveDouble;
using covoca::config::PositiveInt;

/// Filesystem inputs and outputs for one voxel-carving run.
///
/// All paths may be relative; relative paths are resolved against the
/// current working directory the `voxel_carve` binary is run from.
struct VoxelCarvingPaths {
    /// Path to the `gs.calibration.result.v1` YAML file produced by
    /// `aruco_calibrate`. Provides, for every accepted frame, the camera
    /// intrinsics and the `board_to_camera` extrinsics used to project
    /// voxels into that frame.
    std::filesystem::path calibration_result;

    /// Directory containing one binary silhouette mask image per accepted
    /// calibration frame. Each mask must be named after the corresponding
    /// frame image's filename stem with a `.png` extension (e.g. frame
    /// `cam0_0001.jpg` -> mask `cam0_0001.png`). Pixels at or above
    /// `carving.foreground_threshold` are treated as foreground (subject).
    std::filesystem::path masks_dir;

    /// Directory where the exported PLY files are written. Created if it
    /// does not already exist.
    std::filesystem::path output_dir;
};

/// Axis-aligned world-space carving volume.
///
/// Defines the box of 3D space that gets filled with voxels before carving
/// begins. World coordinates use the same convention as the calibration
/// result: the ArUco board frame is the origin.
struct VolumeConfig {
    /// Lower corner of the bounding box, in meters, as `[x, y, z]`.
    Eigen::Vector3d min_m;

    /// Upper corner of the bounding box, in meters, as `[x, y, z]`. Must be
    /// strictly greater than `min_m` along every axis.
    Eigen::Vector3d max_m;

    /// Edge length of each cubic voxel, in meters. Smaller values produce a
    /// finer (but slower and more memory-hungry) reconstruction. The number
    /// of voxels along each axis is `ceil((max_m - min_m) / voxel_size_m)`.
    PositiveDouble voxel_size_m;
};

/// Silhouette consistency settings.
///
/// Controls how each voxel is tested against the per-view silhouette masks
/// to decide whether it should be kept or carved away.
struct CarvingConfig {
    /// Which 3D sample points to test for each voxel:
    ///  - `"center"`: only the voxel's center point.
    ///  - `"corners"`: the voxel's 8 corner points.
    ///  - `"center_and_corners"`: center plus all 8 corners.
    /// More samples make carving stricter (a voxel is kept only if *all*
    /// sampled points project onto foreground in every view).
    SamplePolicy sample_policy;

    /// What to do when a sample point projects outside the image bounds (or
    /// behind the camera) for a given view:
    ///  - `"carve"`: treat the sample as background, i.e. this can cause the
    ///    voxel to be carved.
    ///  - `"keep"`: ignore this sample but still require the rest of the
    ///    voxel's samples in this view to be foreground.
    ///  - `"ignore_view"`: skip this view entirely for this voxel (it cannot
    ///    cause carving, even if other samples in the same view are
    ///    background).
    OutsideImagePolicy outside_image_policy;

    /// Grayscale pixel intensity threshold in `[0, 255]` used when loading
    /// each mask: pixels with intensity >= this value become foreground
    /// (255), everything else becomes background (0).
    int foreground_threshold;

    /// Reserved for future use: the minimum number of views that must agree
    /// a voxel is consistent before it is kept. Currently unused by the
    /// Stage-1 implementation, which checks every available view.
    PositiveInt min_required_views;
};

/// Export output file names, relative to `paths.output_dir`.
struct ExportConfig {
    /// Filename for the ASCII PLY point cloud containing the center of every
    /// occupied (surviving) voxel.
    std::filesystem::path occupied_points_ply;

    /// Filename for the ASCII PLY mesh containing one cube (8 vertices, 12
    /// triangles) per occupied voxel.
    std::filesystem::path occupied_mesh_ply;
};

/// Open3D parity benchmark settings.
struct BenchmarkConfig {
    /// Reserved for future use: when true, also run the Open3D
    /// `carve_silhouette` reference implementation with the same inputs and
    /// report occupied-voxel-count/IoU/runtime differences. Not yet
    /// implemented by `voxel_carve`.
    bool open3d_enabled = false;
};

/// Full voxel-carving configuration loaded from a YAML/JSON file.
///
/// See `configs/voxel_carving/sample.yaml` for a complete example.
struct VoxelCarvingConfig {
    /// Config schema identifier; must be the literal string
    /// `"gs.voxel_carving.config.v1"`.
    ConfigSchema schema;

    /// Human-readable name for this run, used only for logging/bookkeeping.
    std::string name;

    /// Input and output filesystem paths. See `VoxelCarvingPaths`.
    VoxelCarvingPaths paths;

    /// World-space volume to carve. See `VolumeConfig`.
    VolumeConfig volume;

    /// Silhouette consistency check settings. See `CarvingConfig`.
    CarvingConfig carving;

    /// Output PLY filenames. Named `export_config` in C++ (and accessed via
    /// `.get()`) because `export` is a reserved C++ keyword, but the YAML/JSON
    /// key is `export`. See `ExportConfig`.
    rfl::Rename<"export", ExportConfig> export_config;

    /// Optional Open3D benchmark settings. See `BenchmarkConfig`.
    BenchmarkConfig benchmark;
};

/**
 * @brief Loads and validates voxel-carving config.
 *
 * Args:
 *   path: YAML or JSON config path.
 *
 * Returns:
 *   Validated config.
 */
VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path& path);

/**
 * @brief Validates voxel-carving config.
 *
 * Args:
 *   config: Configuration to validate.
 */
void validateVoxelCarvingConfig(const VoxelCarvingConfig& config);

} // namespace covoca::voxel_carving
