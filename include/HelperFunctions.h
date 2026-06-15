#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

#include "Camera.h"

namespace fs = std::filesystem;

// Flattened index to access voxelgrid matrix
int calculateFlattenedIndex(const Eigen::Vector3i &size,
                            int x,
                            int y,
                            int z);

void visualizeSilhouette(
    const cv::Mat &original,
    const cv::Mat &silhouette,
    const std::string &windowName = "Silhouette Check");

// Loads all images into a vector of cv::Mat
std::vector<cv::Mat> loadImages(std::string _folder);

// Load cameras intrinsics and extrinsics from dino_par.txt
std::vector<Camera> loadCameras(const std::string &filename);