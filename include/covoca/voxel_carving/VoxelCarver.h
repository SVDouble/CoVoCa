#pragma once

#include <cstddef>
#include <span>

#include "covoca/voxel_carving/CalibratedView.h"
#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/// Voxel-count and timing summary for one carving run.
struct CarvingStats {
    std::size_t inputVoxelCount = 0;
    std::size_t occupiedVoxelCount = 0;
    std::size_t carvedVoxelCount = 0;
    double elapsedSeconds = 0.0;
};

/// Carved volume and its summary statistics.
struct VoxelCarvingResult {
    VoxelVolume volume;
    CarvingStats stats;
};

/// Carves a voxel volume from calibrated silhouette views.
class VoxelCarver {
  public:
    /**
     * Builds a carver from a voxel-carving config.
     *
     * Args:
     *   config: Volume bounds, voxel size, and carving settings.
     */
    explicit VoxelCarver(VoxelCarvingConfig config);

    /**
     * Builds the voxel volume and carves it against every view.
     *
     * Creates a dense `VoxelVolume` over `config.volume`, then for every
     * voxel checks `SilhouetteConsistency::isConsistent` and clears
     * occupancy for voxels that fail the check.
     *
     * Args:
     *   views: Calibrated views to carve against.
     *
     * Returns:
     *   The carved volume together with input/occupied/carved voxel counts
     *   and the elapsed time.
     */
    [[nodiscard]] VoxelCarvingResult run(std::span<const CalibratedView> views) const;

  private:
    VoxelCarvingConfig config_;
};

} // namespace covoca::voxel_carving
