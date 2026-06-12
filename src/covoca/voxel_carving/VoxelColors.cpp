#include "covoca/voxel_carving/VoxelColors.h"

namespace covoca::voxel_carving {

VoxelColors VoxelColors::create(std::size_t voxelCount) {
    VoxelColors colors;
    colors.colors_.assign(voxelCount, Rgb{0, 0, 0});
    return colors;
}

void VoxelColors::set(std::size_t flatIndex, const Rgb& rgb) {
    colors_.at(flatIndex) = rgb;
}

const Rgb& VoxelColors::at(std::size_t flatIndex) const {
    return colors_.at(flatIndex);
}

} // namespace covoca::voxel_carving
