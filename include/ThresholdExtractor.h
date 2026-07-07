#pragma once

#include "ExtractorConfigs.h"
#include "SilhouetteExtractor.h"
#include <opencv2/opencv.hpp>

class ThresholdExtractor : public SilhouetteExtractor {
public:
  explicit ThresholdExtractor(const ThresholdConfig &config);

  cv::Mat extract(const cv::Mat &inputImage) override;

  std::string name() const override;

private:
  ThresholdConfig m_config;
};
