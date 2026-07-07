#pragma once

#include <opencv2/opencv.hpp>

#include "Camera.h"

struct ObjectView {
  Camera camera;
  cv::Mat silhouette;
  cv::Mat color_image;
};
