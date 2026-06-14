#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "VoxelGrid.h"
#include "Voxel.h"
#include "Camera.h"
#include "View.h"

class VoxelCarver
{
public:
    // Constructor
    VoxelCarver(VoxelGrid _voxel_grid,
                std::vector<cv::Mat> _silhouette_vector,
                std::vector<Camera> _camera_vector);

    // Getters
    const VoxelGrid &getVoxelGrid() const;
    const std::vector<View> &getViewVector() const;

    // Carving method
    void carve();

private:
    VoxelGrid m_voxel_grid;
    std::vector<View> m_view_vector;
};

