#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace covoca::voxel_carving {

/// A color in `(red, green, blue)` order, each channel in `[0, 255]`. Used
/// throughout the color-reconstruction pipeline, from `ColorImage::colorAt`
/// to `VoxelColors` storage, so colors pass through without conversion.
using Rgb = std::array<std::uint8_t, 3>;

/// Per-voxel RGB colors, aligned with `VoxelVolume`'s flat occupancy storage.
///
/// Voxels with no assigned color default to black `(0, 0, 0)`.
class VoxelColors {
  public:
    /**
     * Creates a black-initialized color buffer.
     *
     * Args:
     *   voxelCount: Number of voxels, matching `VoxelVolume::voxelCount()`.
     *
     * Returns:
     *   A buffer with one `(0, 0, 0)` entry per voxel.
     */
    static VoxelColors create(std::size_t voxelCount);

    /**
     * Sets the color of one voxel.
     *
     * Args:
     *   flatIndex: Flat voxel index, as returned by `VoxelVolume::flatIndex`.
     *   rgb: Color in `(red, green, blue)` order, each in `[0, 255]`.
     */
    void set(std::size_t flatIndex, const Rgb& rgb);

    /**
     * Reads the color of one voxel.
     *
     * Args:
     *   flatIndex: Flat voxel index, as returned by `VoxelVolume::flatIndex`.
     *
     * Returns:
     *   The voxel's color in `(red, green, blue)` order.
     */
    [[nodiscard]] const Rgb& at(std::size_t flatIndex) const;

  private:
    std::vector<Rgb> colors_;
};

} // namespace covoca::voxel_carving
