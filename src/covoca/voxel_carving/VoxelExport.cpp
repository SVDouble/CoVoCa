#include "covoca/voxel_carving/VoxelExport.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace covoca::voxel_carving {
namespace {

/**
 * @brief Opens a file for writing, creating parent directories first.
 *
 * Args:
 *   path: Output file path.
 *
 * Returns:
 *   Open output stream.
 */
std::ofstream openOutput(const std::filesystem::path& path) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream stream(path);
    if (!stream) {
        throw std::runtime_error("failed to open " + path.string() + " for writing");
    }
    return stream;
}

/**
 * @brief Collects the grid coordinates of all occupied voxels.
 *
 * Args:
 *   volume: Volume to scan.
 *
 * Returns:
 *   Grid coordinates of every voxel for which `isOccupied()` is true.
 */
std::vector<GridIndex> occupiedIndices(const VoxelVolume& volume) {
    std::vector<GridIndex> indices;
    for (std::size_t flat = 0; flat < volume.voxelCount(); ++flat) {
        const GridIndex index = volume.gridIndex(flat);
        if (volume.isOccupied(index)) {
            indices.push_back(index);
        }
    }
    return indices;
}

} // namespace

void writeOccupiedPointsPly(const std::filesystem::path& path, const VoxelVolume& volume) {
    const std::vector<GridIndex> indices = occupiedIndices(volume);

    std::ofstream stream = openOutput(path);
    stream << "ply\n"
              "format ascii 1.0\n"
              "element vertex "
           << indices.size()
           << "\n"
              "property float x\n"
              "property float y\n"
              "property float z\n"
              "end_header\n";

    for (const GridIndex& index : indices) {
        const Eigen::Vector3d center = volume.center(index);
        stream << center.x() << " " << center.y() << " " << center.z() << "\n";
    }
}

void writeOccupiedCubeMeshPly(const std::filesystem::path& path, const VoxelVolume& volume) {
    const std::vector<GridIndex> indices = occupiedIndices(volume);

    // Quads over the 8 corners returned by VoxelVolume::corners(), indexed by dz*4 + dy*2 + dx.
    constexpr std::array<std::array<int, 4>, 6> kFaces{{
        {0, 1, 3, 2}, // z = 0
        {4, 5, 7, 6}, // z = 1
        {0, 1, 5, 4}, // y = 0
        {2, 3, 7, 6}, // y = 1
        {0, 2, 6, 4}, // x = 0
        {1, 3, 7, 5}, // x = 1
    }};

    std::ofstream stream = openOutput(path);
    stream << "ply\n"
              "format ascii 1.0\n"
              "element vertex "
           << (indices.size() * 8)
           << "\n"
              "property float x\n"
              "property float y\n"
              "property float z\n"
              "element face "
           << (indices.size() * kFaces.size() * 2)
           << "\n"
              "property list uchar int vertex_indices\n"
              "end_header\n";

    for (const GridIndex& index : indices) {
        for (const Eigen::Vector3d& corner : volume.corners(index)) {
            stream << corner.x() << " " << corner.y() << " " << corner.z() << "\n";
        }
    }

    for (std::size_t voxel = 0; voxel < indices.size(); ++voxel) {
        const int base = static_cast<int>(voxel * 8);
        for (const auto& face : kFaces) {
            stream << "3 " << (base + face[0]) << " " << (base + face[1]) << " " << (base + face[2]) << "\n";
            stream << "3 " << (base + face[0]) << " " << (base + face[2]) << " " << (base + face[3]) << "\n";
        }
    }
}

} // namespace covoca::voxel_carving
