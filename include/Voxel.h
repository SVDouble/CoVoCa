#pragma once

#include <iostream>
#include <Eigen/Dense>

class Voxel
{
public:
    // Constructors
    Voxel()
    {
        m_cartesian_pos = Eigen::Vector3d(0.0, 0.0, 0.0);
        m_index_pos = Eigen::Vector3i(-1, -1, -1);
    }

    Voxel(Eigen::Vector3d _cartesian_pos, Eigen::Vector3i _index_pos)
    {
        m_cartesian_pos = _cartesian_pos;
        m_index_pos = _index_pos;
    }

    // Getters (keine Ahnung wie das eigentlich heisst)
    Eigen::Vector3d getCartesianPos()
    {
        return m_cartesian_pos;
    }

    Eigen::Vector3i getIndexPos()
    {
        return m_index_pos;
    }

private:
    Eigen::Vector3d m_cartesian_pos;
    Eigen::Vector3i m_index_pos; // indexes in Voxel grid [row, coloumn, depth], initialize at [-1, -1, -1]
};
