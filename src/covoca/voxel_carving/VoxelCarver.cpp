#include "covoca/voxel_carving/VoxelCarver.h"

#include <chrono>
#include <utility>

#include "covoca/voxel_carving/SilhouetteConsistency.h"

namespace covoca::voxel_carving {

/**
 * Stores the configuration for later carving runs.
 *
 * Args:
 *   config: Voxel-carving configuration.
 */
VoxelCarver::VoxelCarver(VoxelCarvingConfig config) : config_(std::move(config)) {}

/**
 * Builds and carves the configured voxel volume.
 *
 * Args:
 *   views: Calibrated silhouette views used for consistency checks.
 *
 * Returns:
 *   Carved volume plus voxel-count and timing statistics.
 */
VoxelCarvingResult VoxelCarver::run(std::span<const CalibratedView> views) const {
    const auto start = std::chrono::steady_clock::now();

    VoxelVolume volume =
        VoxelVolume::create(config_.volume.min_m, config_.volume.max_m, config_.volume.voxel_size_m.value());
    const SilhouetteConsistency consistency(config_.carving);

    CarvingStats stats;
    stats.inputVoxelCount = volume.voxelCount();

    for (std::size_t flat = 0; flat < volume.voxelCount(); ++flat) {
        const GridIndex index = volume.gridIndex(flat);
        if (!consistency.isConsistent(volume, index, views)) {
            volume.setOccupied(index, false);
            ++stats.carvedVoxelCount;
        } else {
            ++stats.occupiedVoxelCount;
        }
    }

    stats.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    return VoxelCarvingResult{.volume = std::move(volume), .stats = stats};
}

} // namespace covoca::voxel_carving
