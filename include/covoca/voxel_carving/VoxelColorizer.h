#pragma once

#include <span>

#include "covoca/voxel_carving/CalibratedView.h"
#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelColors.h"
#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/// Assigns an RGB color to every occupied voxel of a carved volume.
///
/// For each occupied voxel, gathers one `ColorSample` per view whose
/// projection of the voxel's center lands on foreground and has a loaded
/// color image, then combines the samples with the `ColorReconstructor`
/// selected by `ColorConfig::method`. Voxels with no contributing view (and
/// all carved-away voxels) keep the default black color.
class VoxelColorizer {
  public:
    /**
     * Builds a colorizer from color-reconstruction settings.
     *
     * Args:
     *   config: Selects the reconstruction method and output filenames.
     */
    explicit VoxelColorizer(ColorConfig config);

    /**
     * Computes one color per voxel.
     *
     * Args:
     *   volume: Carved volume; only occupied voxels are colored.
     *   views: Calibrated views with loaded color images.
     *
     * Returns:
     *   Per-voxel colors, aligned with `volume`'s flat indexing.
     */
    [[nodiscard]] VoxelColors run(const VoxelVolume& volume, std::span<const CalibratedView> views) const;

  private:
    ColorConfig config_;
};

} // namespace covoca::voxel_carving
