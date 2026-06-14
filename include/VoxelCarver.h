#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "VoxelGrid.h"
#include "Camera.h"

class VoxelCarver
{
public:
    // Constructors
    VoxelCarver(VoxelGrid _voxel_grid,
                std::vector<cv::Mat> _silhouette_vector,
                std::vector<Camera> _camera_vector);

    // Getters
    const VoxelGrid &getVoxelGrid() const;
    const std::vector<cv::Mat> &getSilhouetteVector() const;
    const std::vector<Camera> &getCameraVector() const;

private:
    VoxelGrid m_voxel_grid;
    std::vector<cv::Mat> m_silhouette_vector;
    std::vector<Camera> m_camera_vector;
};