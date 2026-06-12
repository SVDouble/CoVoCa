#include "covoca/voxel_carving/VoxelCarvingConfig.h"

#include <stdexcept>

#include "covoca/config/ConfigIO.h"

namespace covoca::voxel_carving {

/**
 * Loads a voxel-carving config and applies domain validation.
 *
 * Args:
 *   path: YAML or JSON config path.
 *
 * Returns:
 *   Validated voxel-carving config.
 */
VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path& path) {
    auto config = covoca::config::loadTypedConfig<VoxelCarvingConfig>(path, "voxel carving config");
    validateVoxelCarvingConfig(config);
    return config;
}

/**
 * Validates constraints that are not expressible in the typed model.
 *
 * Args:
 *   config: Parsed voxel-carving config.
 *
 * Throws:
 *   std::runtime_error: If a path field is empty, the volume bounds are
 *   invalid, the mask threshold is outside `[0, 255]`, or an output filename
 *   is missing.
 */
void validateVoxelCarvingConfig(const VoxelCarvingConfig& config) {
    if (config.name.empty()) {
        throw std::runtime_error("config field must not be empty: name");
    }
    if (config.paths.calibration_result.empty()) {
        throw std::runtime_error("config field must not be empty: paths.calibration_result");
    }
    if (config.paths.masks_dir.empty()) {
        throw std::runtime_error("config field must not be empty: paths.masks_dir");
    }
    if (config.paths.output_dir.empty()) {
        throw std::runtime_error("config field must not be empty: paths.output_dir");
    }
    if ((config.volume.max_m.array() <= config.volume.min_m.array()).any()) {
        throw std::runtime_error("config field volume.max_m must be greater than volume.min_m in every dimension");
    }
    if (config.carving.foreground_threshold < 0 || config.carving.foreground_threshold > 255) {
        throw std::runtime_error("config field carving.foreground_threshold must be in [0, 255]");
    }
    if (config.export_config.get().occupied_points_file.empty()) {
        throw std::runtime_error("config field must not be empty: export.occupied_points_file");
    }
    if (config.export_config.get().occupied_mesh_file.empty()) {
        throw std::runtime_error("config field must not be empty: export.occupied_mesh_file");
    }
    if (config.color) {
        if (config.color->occupied_points_file.empty()) {
            throw std::runtime_error("config field must not be empty: color.occupied_points_file");
        }
        if (config.color->occupied_mesh_file.empty()) {
            throw std::runtime_error("config field must not be empty: color.occupied_mesh_file");
        }
    }
}

} // namespace covoca::voxel_carving
