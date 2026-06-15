#pragma once

#include <Eigen/Dense>

class Voxel
{
public:
    // Constructors
    Voxel();
    Voxel(Eigen::Vector3d _cartesian_pos, Eigen::Vector3i _index_pos);
    Voxel(Eigen::Vector3d _cartesian_pos, Eigen::Vector3i _index_pos, bool _occupied);

    // Getters
    Eigen::Vector3d getCartesianPos() const;
    Eigen::Vector3i getIndexPos() const;
    bool getOccupied() const;

    // Setters
    void setOccupied(bool _new_occupied);

private:
    Eigen::Vector3d m_cartesian_pos; // Cartesian position of voxel center
    Eigen::Vector3i m_index_pos;     // Indices in voxel grid [row, column, depth]
    bool m_occupied;
};