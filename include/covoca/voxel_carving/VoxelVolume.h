#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Eigen/Dense>

namespace covoca::voxel_carving {

/// 3D grid coordinates of a voxel.
struct GridIndex {
    int x = 0;
    int y = 0;
    int z = 0;
};

/// Dense, axis-aligned voxel volume with per-voxel occupancy.
///
/// Voxels are initially occupied. Carving clears occupancy.
/// Flat storage uses index order `(z * ny + y) * nx + x`.
class VoxelVolume {
  public:
    /**
     * @brief Builds a dense voxel grid covering an axis-aligned box.
     *
     * Grid dimensions are `ceil((maxCorner - minCorner) / voxelSizeMeters)`
     * per axis, with a minimum of 1 voxel per axis. All voxels start
     * occupied.
     *
     * Args:
     *   minCorner: Lower corner of the box, in meters.
     *   maxCorner: Upper corner of the box, in meters; must be strictly
     *     greater than `minCorner` on every axis.
     *   voxelSizeMeters: Edge length of each cubic voxel, in meters; must be
     *     positive.
     *
     * Returns:
     *   A new volume with every voxel marked occupied.
     */
    static VoxelVolume create(const Eigen::Vector3d& minCorner, const Eigen::Vector3d& maxCorner,
                              double voxelSizeMeters);

    /// Total number of voxels in the grid (`nx * ny * nz`).
    [[nodiscard]] std::size_t voxelCount() const {
        return occupied_.size();
    }

    /// Grid dimensions `(nx, ny, nz)`.
    [[nodiscard]] const Eigen::Vector3i& dims() const {
        return dims_;
    }

    /// Edge length of each cubic voxel, in meters.
    [[nodiscard]] double voxelSize() const {
        return voxelSize_;
    }

    /// Lower corner of the volume, in meters.
    [[nodiscard]] const Eigen::Vector3d& minCorner() const {
        return minCorner_;
    }

    /**
     * @brief Converts a flat storage index to grid coordinates.
     *
     * Args:
     *   flatIndex: Index into the flat occupancy array, using order
     *     `(z * ny + y) * nx + x`.
     *
     * Returns:
     *   The corresponding `(x, y, z)` grid coordinates.
     */
    [[nodiscard]] GridIndex gridIndex(std::size_t flatIndex) const;

    /**
     * @brief Converts grid coordinates to a flat storage index.
     *
     * Args:
     *   index: Grid coordinates.
     *
     * Returns:
     *   The corresponding flat index, using order `(z * ny + y) * nx + x`.
     */
    [[nodiscard]] std::size_t flatIndex(GridIndex index) const;

    /**
     * @brief Computes the world-space center of a voxel.
     *
     * Args:
     *   index: Grid coordinates of the voxel.
     *
     * Returns:
     *   The voxel's center point, in meters.
     */
    [[nodiscard]] Eigen::Vector3d center(GridIndex index) const;

    /**
     * @brief Computes the 8 world-space corners of a voxel.
     *
     * Args:
     *   index: Grid coordinates of the voxel.
     *
     * Returns:
     *   The voxel's corner points, in meters, ordered by `dz * 4 + dy * 2 +
     *   dx` for `dx, dy, dz` in `{0, 1}`.
     */
    [[nodiscard]] std::array<Eigen::Vector3d, 8> corners(GridIndex index) const;

    /**
     * @brief Checks whether a voxel is occupied.
     *
     * Args:
     *   index: Grid coordinates of the voxel.
     *
     * Returns:
     *   True if the voxel has not been carved away.
     */
    [[nodiscard]] bool isOccupied(GridIndex index) const;

    /**
     * @brief Sets a voxel's occupancy.
     *
     * Args:
     *   index: Grid coordinates of the voxel.
     *   occupied: New occupancy state.
     */
    void setOccupied(GridIndex index, bool occupied);

  private:
    Eigen::Vector3d minCorner_ = Eigen::Vector3d::Zero();
    double voxelSize_ = 0.0;
    Eigen::Vector3i dims_ = Eigen::Vector3i::Zero();
    std::vector<std::uint8_t> occupied_;
};

} // namespace covoca::voxel_carving
