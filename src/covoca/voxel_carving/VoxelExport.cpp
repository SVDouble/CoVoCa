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

// Quads over the 8 corners returned by VoxelVolume::corners(), indexed by dz*4 + dy*2 + dx.
constexpr std::array<std::array<int, 4>, 6> kCubeFaces{{
    {0, 1, 3, 2}, // z = 0
    {4, 5, 7, 6}, // z = 1
    {0, 1, 5, 4}, // y = 0
    {2, 3, 7, 6}, // y = 1
    {0, 2, 6, 4}, // x = 0
    {1, 3, 7, 5}, // x = 1
}};

bool isOff(ExportFormat format) {
    return format.value() == ExportFormat::value_of<"off">();
}

} // namespace

void writeOccupiedPoints(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format) {
    const std::vector<GridIndex> indices = occupiedIndices(volume);

    std::ofstream stream = openOutput(path);
    if (isOff(format)) {
        stream << "OFF\n" << indices.size() << " 0 0\n";
    } else {
        stream << "ply\n"
                  "format ascii 1.0\n"
                  "element vertex "
               << indices.size()
               << "\n"
                  "property float x\n"
                  "property float y\n"
                  "property float z\n"
                  "end_header\n";
    }

    for (const GridIndex& index : indices) {
        const Eigen::Vector3d center = volume.center(index);
        stream << center.x() << " " << center.y() << " " << center.z() << "\n";
    }
}

void writeOccupiedCubeMesh(const std::filesystem::path& path, const VoxelVolume& volume, ExportFormat format) {
    const std::vector<GridIndex> indices = occupiedIndices(volume);
    const std::size_t triangleCount = indices.size() * kCubeFaces.size() * 2;

    std::ofstream stream = openOutput(path);
    if (isOff(format)) {
        stream << "OFF\n" << (indices.size() * 8) << " " << triangleCount << " 0\n";
    } else {
        stream << "ply\n"
                  "format ascii 1.0\n"
                  "element vertex "
               << (indices.size() * 8)
               << "\n"
                  "property float x\n"
                  "property float y\n"
                  "property float z\n"
                  "element face "
               << triangleCount
               << "\n"
                  "property list uchar int vertex_indices\n"
                  "end_header\n";
    }

    for (const GridIndex& index : indices) {
        for (const Eigen::Vector3d& corner : volume.corners(index)) {
            stream << corner.x() << " " << corner.y() << " " << corner.z() << "\n";
        }
    }

    for (std::size_t voxel = 0; voxel < indices.size(); ++voxel) {
        const int base = static_cast<int>(voxel * 8);
        for (const auto& face : kCubeFaces) {
            stream << "3 " << (base + face[0]) << " " << (base + face[1]) << " " << (base + face[2]) << "\n";
            stream << "3 " << (base + face[0]) << " " << (base + face[2]) << " " << (base + face[3]) << "\n";
        }
    }
}

} // namespace covoca::voxel_carving
