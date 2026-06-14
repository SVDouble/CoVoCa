#include "VoxelGrid.h"

VoxelGrid::VoxelGrid(int _size, double _step_size)
    : m_size(_size),
      m_step_size(_step_size),
      m_voxel_flattened_matrix(_size * _size * _size)
{
    double offset = (_size - 1) * 0.5 * _step_size;

    for (int z = 0; z < m_size; ++z)
    {
        for (int y = 0; y < m_size; ++y)
        {
            for (int x = 0; x < m_size; ++x)
            {
                int index = x + m_size * (y + m_size * z);

                double px = x * _step_size - offset;
                double py = y * _step_size - offset;
                double pz = z * _step_size - offset;

                Eigen::Vector3d cartesian_pos(px, py, pz);
                Eigen::Vector3i index_pos(x, y, z);

                m_voxel_flattened_matrix[index] =
                    Voxel(cartesian_pos, index_pos);
            }
        }
    }
}

int VoxelGrid::getSize() const
{
    return m_size;
}

const std::vector<Voxel> &VoxelGrid::getVoxelGrid() const
{
    return m_voxel_flattened_matrix;
}

double VoxelGrid::getStepSize() const
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