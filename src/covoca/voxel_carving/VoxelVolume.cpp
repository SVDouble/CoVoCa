#include "covoca/voxel_carving/VoxelVolume.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace covoca::voxel_carving {

VoxelVolume VoxelVolume::create(const Eigen::Vector3d& minCorner, const Eigen::Vector3d& maxCorner,
                                double voxelSizeMeters) {
    if (voxelSizeMeters <= 0.0) {
        throw std::invalid_argument("voxel size must be positive");
    }
    if ((maxCorner.array() <= minCorner.array()).any()) {
        throw std::invalid_argument("volume max_m must be greater than min_m in every dimension");
    }

    VoxelVolume volume;
    volume.minCorner_ = minCorner;
    volume.voxelSize_ = voxelSizeMeters;

    const Eigen::Vector3d extent = maxCorner - minCorner;
    for (int axis = 0; axis < 3; ++axis) {
        volume.dims_[axis] = std::max(1, static_cast<int>(std::ceil(extent[axis] / voxelSizeMeters)));
    }

    const auto count = static_cast<std::size_t>(volume.dims_.x()) * static_cast<std::size_t>(volume.dims_.y()) *
                       static_cast<std::size_t>(volume.dims_.z());
    volume.occupied_.assign(count, 1);
    return volume;
}

GridIndex VoxelVolume::gridIndex(std::size_t flatIndex) const {
    const auto nx = static_cast<std::size_t>(dims_.x());
    const auto ny = static_cast<std::size_t>(dims_.y());
    const std::size_t x = flatIndex % nx;
    const std::size_t y = (flatIndex / nx) % ny;
    const std::size_t z = flatIndex / (nx * ny);
    return GridIndex{.x = static_cast<int>(x), .y = static_cast<int>(y), .z = static_cast<int>(z)};
}

std::size_t VoxelVolume::flatIndex(GridIndex index) const {
    const auto nx = static_cast<std::size_t>(dims_.x());
    const auto ny = static_cast<std::size_t>(dims_.y());
    return (((static_cast<std::size_t>(index.z) * ny) + static_cast<std::size_t>(index.y)) * nx) +
           static_cast<std::size_t>(index.x);
}

Eigen::Vector3d VoxelVolume::center(GridIndex index) const {
    return minCorner_ +
           Eigen::Vector3d{(index.x + 0.5) * voxelSize_, (index.y + 0.5) * voxelSize_, (index.z + 0.5) * voxelSize_};
}

std::array<Eigen::Vector3d, 8> VoxelVolume::corners(GridIndex index) const {
    const Eigen::Vector3d origin =
        minCorner_ + Eigen::Vector3d{index.x * voxelSize_, index.y * voxelSize_, index.z * voxelSize_};
    // Order matches the corner index dz*4 + dy*2 + dx used by VoxelExport's face table.
    std::array<Eigen::Vector3d, 8> result;
    std::size_t i = 0;
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                result[i++] = origin + Eigen::Vector3d{dx * voxelSize_, dy * voxelSize_, dz * voxelSize_};
            }
        }
    }
    return result;
}

bool VoxelVolume::isOccupied(GridIndex index) const {
    return occupied_.at(flatIndex(index)) != 0;
}

void VoxelVolume::setOccupied(GridIndex index, bool occupied) {
    occupied_.at(flatIndex(index)) = occupied ? 1 : 0;
}

} // namespace covoca::voxel_carving
