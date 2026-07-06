#pragma once

#include <filesystem>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Camera.h"

struct LoadedDataset
{
    std::vector<Camera> cameras;
    std::vector<cv::Mat> silhouettes;
    std::vector<cv::Mat> color_images;
};

LoadedDataset loadDataset(const std::filesystem::path &config_path);
