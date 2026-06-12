#pragma once

#include <filesystem>

#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/**
 * @brief Writes occupied voxel centers as an ASCII point cloud.
 *
 * Args:
 *   path: Output file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and centers from.
 *   format: `"ply"` for ASCII PLY, or `"off"` for ASCII OFF (zero faces).
 */
void writeOccupiedPoints(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format);

/**
 * @brief Writes occupied voxels as an ASCII cube mesh.
 *
 * Each occupied voxel becomes a cube: 8 vertices (from
 * `VoxelVolume::corners`) and 12 triangles (2 per face).
 *
 * Args:
 *   path: Output file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and corners from.
 *   format: `"ply"` for ASCII PLY, or `"off"` for ASCII OFF.
 */
void writeOccupiedCubeMesh(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format);

} // namespace covoca::voxel_carving
