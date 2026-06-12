#include "gsplat/voxel/VoxelGrid.h"

#include <cstddef>
#include <stdexcept>

namespace gs::voxel {
namespace {

std::size_t flatIndex(int size, int x, int y, int z) {
    const auto side = static_cast<std::size_t>(size);
    return (((static_cast<std::size_t>(z) * side) + static_cast<std::size_t>(y)) * side) + static_cast<std::size_t>(x);
}

} // namespace

VoxelGrid::VoxelGrid(int size, double stepSize) : size_(size), stepSize_(stepSize) {
    if (size <= 0) {
        throw std::invalid_argument("voxel grid size must be positive");
    }
    if (stepSize <= 0.0) {
        throw std::invalid_argument("voxel grid step size must be positive");
    }

    const auto side = static_cast<std::size_t>(size_);
    voxels_.resize(side * side * side);
    const double offset = (size - 1) * 0.5 * stepSize;
    for (int z = 0; z < size_; ++z) {
        for (int y = 0; y < size_; ++y) {
            for (int x = 0; x < size_; ++x) {
                voxels_[flatIndex(size_, x, y, z)] =
                    Voxel(Eigen::Vector3d{(x * stepSize) - offset, (y * stepSize) - offset, (z * stepSize) - offset},
                          Eigen::Vector3i{x, y, z});
            }
        }
    }
}

const Voxel& VoxelGrid::at(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 || x >= size_ || y >= size_ || z >= size_) {
        throw std::out_of_range("voxel index out of range");
    }
    return voxels_.at(flatIndex(size_, x, y, z));
}

} // namespace gs::voxel
