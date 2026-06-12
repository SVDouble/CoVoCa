#pragma once

#include <filesystem>

#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelColors.h"
#include "covoca/voxel_carving/VoxelVolume.h"

namespace covoca::voxel_carving {

/**
 * Writes occupied voxel centers as an ASCII point cloud.
 *
 * Args:
 *   path: Output file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and centers from.
 *   format: `"ply"` for ASCII PLY, or `"off"` for ASCII OFF (zero faces).
 *   colors: If non-null, per-voxel RGB colors written alongside each vertex
 *     (PLY `red`/`green`/`blue` properties, or OFF `COFF` color columns).
 */
void writeOccupiedPoints(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format,
                         const VoxelColors* colors = nullptr);

/**
 * Writes occupied voxels as an ASCII cube mesh.
 *
 * Each occupied voxel becomes a cube: 8 vertices (from
 * `VoxelVolume::corners`) and 12 triangles (2 per face).
 *
 * Args:
 *   path: Output file path; parent directories are created if needed.
 *   volume: Volume to read occupancy and corners from.
 *   format: `"ply"` for ASCII PLY, or `"off"` for ASCII OFF.
 *   colors: If non-null, per-voxel RGB colors written for every vertex of
 *     that voxel's cube (PLY `red`/`green`/`blue` properties, or OFF `COFF`
 *     color columns).
 */
void writeOccupiedCubeMesh(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format,
                           const VoxelColors* colors = nullptr);

} // namespace covoca::voxel_carving
