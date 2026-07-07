#pragma once

#include "ExtractorConfigs.h"
#include "SilhouetteExtractor.h"
#include <opencv2/aruco.hpp>
#include <opencv2/opencv.hpp>

class PlanarHomographyExtractor : public SilhouetteExtractor {
public:
  explicit PlanarHomographyExtractor(const PlanarHomographyConfig &config);
  cv::Mat extract(const cv::Mat &inputImage) override;
  std::string name() const override;

private:
  cv::Mat createMarkerMask(const cv::Size &size);
  cv::Mat createBoardMask(const cv::Size &size);

  PlanarHomographyConfig m_config;
  cv::aruco::Dictionary m_dictionary;
  cv::Mat m_refGray;
  std::unordered_map<int, std::vector<cv::Point2f>> m_refMarkerCorners;
  cv::Mat m_boardMask;
};
