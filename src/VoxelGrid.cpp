#include <fstream>

#include "VoxelGrid.h"

VoxelGrid::VoxelGrid(Eigen::Vector3d _bounding_box_min,
                     Eigen::Vector3d _bounding_box_max,
                     Eigen::Vector3i _size)
    : m_size(_size),
      m_center((_bounding_box_min + _bounding_box_max) * 0.5)
{
    int nx = _size.x();
    int ny = _size.y();
    int nz = _size.z();

    m_voxel_flattened_matrix.resize(nx * ny * nz);

    Eigen::Vector3d dimensions = _bounding_box_max - _bounding_box_min;

    m_step_size.x() = dimensions.x() / (nx - 1);
    m_step_size.y() = dimensions.y() / (ny - 1);
    m_step_size.z() = dimensions.z() / (nz - 1);

    for (int z = 0; z < nz; ++z)
        for (int y = 0; y < ny; ++y)
            for (int x = 0; x < nx; ++x)
            {
                int index = x + nx * (y + ny * z);

                Eigen::Vector3d position;
                position.x() = _bounding_box_min.x() + x * m_step_size.x();
                position.y() = _bounding_box_min.y() + y * m_step_size.y();
                position.z() = _bounding_box_min.z() + z * m_step_size.z();

                m_voxel_flattened_matrix[index] =
                    Voxel(position, Eigen::Vector3i(x, y, z));
            }
}

Eigen::Vector3i VoxelGrid::getSize() const
{
    return m_size;
}

const std::vector<Voxel> &VoxelGrid::getVoxelGrid() const
{
    return m_voxel_flattened_matrix;
}

Eigen::Vector3d VoxelGrid::getStepSize() const
{
    return m_step_size;
}

const Voxel &VoxelGrid::getVoxel(int _row, int _column, int _depth) const
{
    return m_voxel_flattened_matrix[calculateFlattenedIndex(m_size, _row, _column, _depth)];
}

void VoxelGrid::setVoxelOccupied(bool _occupied, int _index)
{
    m_voxel_flattened_matrix[_index].setOccupied(_occupied);
}

void VoxelGrid::saveVoxelGrid()
{
    std::ofstream file("voxel_grid.ply");

    if (!file.is_open())
    {
        std::cerr << "Could not create voxel_grid.ply\n";
        return;
    }

    // Count occupied voxels for file header
    int occupied_count = 0;

    for (const auto &voxel : m_voxel_flattened_matrix)
    {
        if (voxel.getOccupied())
        {
            occupied_count++;
        }
    }

    // Header
    file << "ply\n"; // PLY format
    file << "format ascii 1.0\n";
    file << "element vertex " << occupied_count << "\n"; // number of vertexes
    file << "property float x\n";                        // vertexes defined by x,y,z position
    file << "property float y\n";
    file << "property float z\n";
    file << "end_header\n";

    // Write voxel centers
    for (const auto &voxel : m_voxel_flattened_matrix)
    {
        if (!voxel.getOccupied())
            continue;

        Eigen::Vector3d pos = voxel.getCartesianPos();

        file << pos.x() << " "
             << pos.y() << " "
             << pos.z() << "\n";
    }

    file.close();

    std::cout << "Saved " << occupied_count
              << " occupied voxels to voxel_grid.ply\n";
}