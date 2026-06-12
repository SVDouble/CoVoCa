#include "covoca/voxel_carving/VoxelColors.h"

namespace covoca::voxel_carving {

/**
 * Creates a per-voxel color buffer initialized to black.
 *
 * Args:
 *   voxelCount: Number of voxel color entries to allocate.
 *
 * Returns:
 *   Color buffer with `voxelCount` black entries.
 */
VoxelColors VoxelColors::create(std::size_t voxelCount) {
    VoxelColors colors;
    colors.colors_.assign(voxelCount, Rgb{0, 0, 0});
    return colors;
}

/**
 * Assigns one voxel's RGB color.
 *
 * Args:
 *   flatIndex: Flat voxel index.
 *   rgb: Color in `(red, green, blue)` order.
 */
void VoxelColors::set(std::size_t flatIndex, const Rgb& rgb) {
    colors_.at(flatIndex) = rgb;
}

/**
 * Reads one voxel's RGB color.
 *
 * Args:
 *   flatIndex: Flat voxel index.
 *
 * Returns:
 *   Stored color in `(red, green, blue)` order.
 */
const Rgb& VoxelColors::at(std::size_t flatIndex) const {
    return colors_.at(flatIndex);
}

} // namespace covoca::voxel_carving
