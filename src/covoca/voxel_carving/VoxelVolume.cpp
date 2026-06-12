#include "covoca/voxel_carving/VoxelVolume.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace covoca::voxel_carving {

/**
 * Creates an occupied voxel grid over an axis-aligned box.
 *
 * Args:
 *   minCorner: Lower world-space volume corner, in meters.
 *   maxCorner: Upper world-space volume corner, in meters.
 *   voxelSizeMeters: Cubic voxel edge length, in meters.
 *
 * Returns:
 *   Dense volume with every voxel initially marked occupied.
 *
 * Throws:
 *   std::invalid_argument: If the voxel size is non-positive or any max
 *   bound is not greater than the corresponding min bound.
 */
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
    volume.dims_ = GridDimensions{
        .x = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(extent.x() / voxelSizeMeters))),
        .y = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(extent.y() / voxelSizeMeters))),
        .z = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(extent.z() / voxelSizeMeters))),
    };

    const std::size_t count = volume.dims_.x * volume.dims_.y * volume.dims_.z;
    volume.occupied_.assign(count, 1);
    return volume;
}

/**
 * Converts a flat voxel index to `(x, y, z)` grid coordinates.
 *
 * Args:
 *   flatIndex: Index in flat storage order.
 *
 * Returns:
 *   Grid coordinates for `flatIndex`.
 */
GridIndex VoxelVolume::gridIndex(std::size_t flatIndex) const {
    const std::size_t x = flatIndex % dims_.x;
    const std::size_t y = (flatIndex / dims_.x) % dims_.y;
    const std::size_t z = flatIndex / (dims_.x * dims_.y);
    return GridIndex{.x = x, .y = y, .z = z};
}

/**
 * Converts grid coordinates to a flat voxel index.
 *
 * Args:
 *   index: Grid coordinates.
 *
 * Returns:
 *   Flat storage index for `index`.
 */
std::size_t VoxelVolume::flatIndex(GridIndex index) const {
    return (((index.z * dims_.y) + index.y) * dims_.x) + index.x;
}

/**
 * Computes the world-space center of a voxel.
 *
 * Args:
 *   index: Grid coordinates of the voxel.
 *
 * Returns:
 *   Center point in meters.
 */
Eigen::Vector3d VoxelVolume::center(GridIndex index) const {
    return minCorner_ + Eigen::Vector3d{(static_cast<double>(index.x) + 0.5) * voxelSize_,
                                        (static_cast<double>(index.y) + 0.5) * voxelSize_,
                                        (static_cast<double>(index.z) + 0.5) * voxelSize_};
}

/**
 * Computes the world-space cube corners of a voxel.
 *
 * Args:
 *   index: Grid coordinates of the voxel.
 *
 * Returns:
 *   Eight corner points in `dz * 4 + dy * 2 + dx` order.
 */
std::array<Eigen::Vector3d, 8> VoxelVolume::corners(GridIndex index) const {
    const Eigen::Vector3d origin = minCorner_ + Eigen::Vector3d{static_cast<double>(index.x) * voxelSize_,
                                                                static_cast<double>(index.y) * voxelSize_,
                                                                static_cast<double>(index.z) * voxelSize_};
    // Order matches the corner index dz*4 + dy*2 + dx used by VoxelExport's face table.
    std::array<Eigen::Vector3d, 8> result;
    std::size_t i = 0;
    for (std::size_t dz = 0; dz < 2; ++dz) {
        for (std::size_t dy = 0; dy < 2; ++dy) {
            for (std::size_t dx = 0; dx < 2; ++dx) {
                result[i++] =
                    origin + Eigen::Vector3d{static_cast<double>(dx) * voxelSize_, static_cast<double>(dy) * voxelSize_,
                                             static_cast<double>(dz) * voxelSize_};
            }
        }
    }
    return result;
}

/**
 * Checks whether a voxel is currently occupied.
 *
 * Args:
 *   index: Grid coordinates of the voxel.
 *
 * Returns:
 *   True if the voxel has not been carved away.
 */
bool VoxelVolume::isOccupied(GridIndex index) const {
    return occupied_.at(flatIndex(index)) != 0;
}

/**
 * Updates a voxel's occupancy state.
 *
 * Args:
 *   index: Grid coordinates of the voxel.
 *   occupied: New occupancy value.
 */
void VoxelVolume::setOccupied(GridIndex index, bool occupied) {
    occupied_.at(flatIndex(index)) = occupied ? 1 : 0;
}

} // namespace covoca::voxel_carving
