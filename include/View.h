#pragma once

#include <opencv2/opencv.hpp>

#include "Camera.h"

struct View
{
    Camera camera;
    cv::Mat silhouette;
    cv::Mat colorImage; // Original color image for color reconstruction
};