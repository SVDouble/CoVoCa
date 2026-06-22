#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

struct ThresholdConfig {
  int diffThreshold = 30;
};

struct PlanarHomographyConfig {
  cv::Mat referenceImage;

  std::vector<std::vector<cv::Point2f>> referenceCorners;
  std::vector<int> referenceIds;

  int arucoDictionaryId = cv::aruco::DICT_6X6_250;

  int diffThreshold = 30;
  bool useShadowDetection = true;
  float shadowBrightnessRatio = 0.7f;

  //TODO: morphological parameters?

  int minMarkersRequired = 4;
};


