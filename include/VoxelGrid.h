#pragma once

#include <vector>
#include <Eigen/Dense>

#include "Voxel.h"
#include "HelperFunctions.h"

class VoxelGrid
{
public:
    // Constructor for voxel grid
    VoxelGrid(Eigen::Vector3d _bounding_box_min, Eigen::Vector3d _bounding_box_max, Eigen::Vector3i _size);

    // Getters
    Eigen::Vector3i getSize() const;
    const std::vector<Voxel> &getVoxelGrid() const;
    Eigen::Vector3d getStepSize() const;
    const Voxel &getVoxel(int _row, int _column, int _depth) const;

    // Setter
    void setVoxelOccupied(bool _occupied, int _index);
    void setVoxelColor(Eigen::Vector3i &_color, int _index);

    // Save for MeshLab

    void saveVoxelGrid();

    void saveHullMesh();

private:
    bool isOccupied(int x,
                    int y,
                    int z) const;

    Eigen::Vector3i m_size;
    std::vector<Voxel> m_voxel_flattened_matrix; // Size n³
    Eigen::Vector3d m_step_size;                 // Distance between neighboring voxels
    Eigen::Vector3d m_center;
};