#pragma once

#include <iostream>
#include <Eigen/Dense>

#include "Voxel.h"
#include "HelperFunctions.h"

class VoxelGrid
{
public:
    // Constructor for voxelgrid centered on (0,0,0)
    VoxelGrid(int _size, double _step_size)
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

                    m_voxel_flattened_matrix[index] = Voxel(cartesian_pos, index_pos);
                }
            }
        }
    }

    // Getters
    int getSize()
    {
        return m_size;
    }

    std::vector<Voxel> getVoxelGrid()
    {
        return m_voxel_flattened_matrix;
    }

    double getStepSize()
    {
        return m_step_size;
    }

    Voxel getVoxel(int _row, int _coloumn, int _depth)
    {
        return m_voxel_flattened_matrix[calculateFlattenedIndex(m_size, _row, _coloumn, _depth)];
    }

    // Other

private:
    int m_size;                                  // assuming a cube, could be changed for sizeX, sizeY, sizeZ if needed
    std::vector<Voxel> m_voxel_flattened_matrix; // of size n³, index = row_index + n_rows * (coloumn_index + n_coloumns * depth_index)
    double m_step_size;                          // distance between two neighbouring voxels
};
