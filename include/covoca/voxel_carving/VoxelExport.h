#pragma once

#include <filesystem>

#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/**
 * @brief Writes occupied voxel centers as an ASCII PLY point cloud.
 *
 * Args:
 *   path: Output `.ply` file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and centers from.
 */
void writeOccupiedPointsPly(const std::filesystem::path& path, const VoxelVolume& volume);

/**
 * @brief Writes occupied voxels as an ASCII PLY cube mesh.
 *
 * Each occupied voxel becomes a cube: 8 vertices (from
 * `VoxelVolume::corners`) and 12 triangles (2 per face).
 *
 * Args:
 *   path: Output `.ply` file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and corners from.
 */
void writeOccupiedCubeMeshPly(const std::filesystem::path& path, const VoxelVolume& volume);

} // namespace covoca::voxel_carving
