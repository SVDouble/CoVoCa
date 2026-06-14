#include "VoxelCarver.h"

// Constructor
VoxelCarver::VoxelCarver(VoxelGrid _voxel_grid,
                         std::vector<cv::Mat> _silhouette_vector,
                         std::vector<Camera> _camera_vector)
    : m_voxel_grid(std::move(_voxel_grid)),
      m_silhouette_vector(std::move(_silhouette_vector)),
      m_camera_vector(std::move(_camera_vector))
{
}

// Getters
const VoxelGrid &VoxelCarver::getVoxelGrid() const
{
    return m_voxel_grid;
}

const std::vector<cv::Mat> &VoxelCarver::getSilhouetteVector() const
{
    return m_silhouette_vector;
}

const std::vector<Camera> &VoxelCarver::getCameraVector() const
{
    return m_camera_vector;
}