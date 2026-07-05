#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Camera.h"
#include "VoxelGrid.h"

struct LoadedDataset
{
    std::optional<std::string> color_method;
    VoxelGrid voxel_grid;
    std::vector<Camera> cameras;
    std::vector<cv::Mat> silhouettes;
    std::vector<cv::Mat> color_images;
};

LoadedDataset loadDataset(const std::filesystem::path &config_path);
