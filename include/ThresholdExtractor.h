#pragma once

#include "SilhouetteExtractor.h"
#include "ExtractorConfigs.h"
#include <opencv2/opencv.hpp>

class ThresholdExtractor : public SilhouetteExtractor{
public:
  explicit ThresholdExtractor(const ThresholdConfig& config);

  cv::Mat extract(const cv::Mat& inputImage) override;

private:
  ThresholdConfig config;

};
