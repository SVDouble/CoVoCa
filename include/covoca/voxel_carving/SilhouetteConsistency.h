#pragma once

#include <span>

#include "covoca/voxel_carving/CalibratedView.h"
#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/// Decides whether a voxel remains occupied given its projections into every view.
class SilhouetteConsistency {
  public:
    /**
     * @brief Builds a consistency checker from carving settings.
     *
     * Args:
     *   config: Sample policy, outside-image policy, and threshold settings.
     */
    explicit SilhouetteConsistency(CarvingConfig config);

    /**
     * @brief Checks a voxel against every view's silhouette mask.
     *
     * Samples the voxel according to `config.sample_policy`, projects each
     * sample into each view, and applies `config.outside_image_policy` to
     * samples that fall outside a view's image.
     *
     * Args:
     *   volume: Voxel volume the voxel belongs to.
     *   voxel: Grid coordinates of the voxel to check.
     *   views: Calibrated views to check the voxel against.
     *
     * Returns:
     *   False if any view's silhouette mask marks the voxel as background,
     *   i.e. the voxel must be carved away.
     */
    [[nodiscard]] bool isConsistent(const VoxelVolume& volume, GridIndex voxel,
                                    std::span<const CalibratedView> views) const;

  private:
    CarvingConfig config_;
};

} // namespace covoca::voxel_carving
