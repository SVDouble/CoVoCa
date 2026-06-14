#include "VoxelCarver.h"

// Constructor
VoxelCarver::VoxelCarver(VoxelGrid _voxel_grid,
                         std::vector<cv::Mat> _silhouette_vector,
                         std::vector<Camera> _camera_vector)
    : m_voxel_grid(std::move(_voxel_grid))
{
    size_t n = std::min(_silhouette_vector.size(),
                        _camera_vector.size());

    m_view_vector.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        View view;
        view.camera = std::move(_camera_vector[i]);
        view.silhouette = std::move(_silhouette_vector[i]);

        m_view_vector.push_back(std::move(view));
    }
}

// Getters
const VoxelGrid &VoxelCarver::getVoxelGrid() const
{
    return m_voxel_grid;
}

const std::vector<View> &VoxelCarver::getViewVector() const
{
    return m_view_vector;
}
