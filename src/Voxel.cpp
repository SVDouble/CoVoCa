#include "Voxel.h"

Voxel::Voxel()
{
    m_cartesian_pos = Eigen::Vector3d(0.0, 0.0, 0.0);
    m_index_pos = Eigen::Vector3i(-1, -1, -1);
    m_occupied = true;
}

Voxel::Voxel(Eigen::Vector3d _cartesian_pos, Eigen::Vector3i _index_pos)
{
    m_cartesian_pos = _cartesian_pos;
    m_index_pos = _index_pos;
    m_occupied = true;
}

Voxel::Voxel(Eigen::Vector3d _cartesian_pos, Eigen::Vector3i _index_pos, bool _occupied)
{
    m_cartesian_pos = _cartesian_pos;
    m_index_pos = _index_pos;
    m_occupied = _occupied;
}

Eigen::Vector3d Voxel::getCartesianPos() const
{
    return m_cartesian_pos;
}

Eigen::Vector3i Voxel::getIndexPos() const
{
    return m_index_pos;
}

bool Voxel::getOccupied() const
{
    return m_occupied;
}

void Voxel::setOccupied(bool _new_occupied)
{
    m_occupied = _new_occupied;
}
