#pragma once

#include <vector>
#include <Eigen/Dense>

#include "Voxel.h"
#include "HelperFunctions.h"

class VoxelGrid
{
public:
    // Constructor for voxel grid centered on (0,0,0)
    VoxelGrid(int _size, double _step_size);

    // Getters
    int getSize() const;
    const std::vector<Voxel> &getVoxelGrid() const;
    double getStepSize() const;
    const Voxel &getVoxel(int _row, int _column, int _depth) const;

    // Setter
    void setVoxelOccupied(bool _occupied, int _index);

private:
    int m_size;                                  // Assuming a cube
    std::vector<Voxel> m_voxel_flattened_matrix; // Size n³
    double m_step_size;                          // Distance between neighboring voxels
};