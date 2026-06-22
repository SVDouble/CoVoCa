#include "PlanarHomographyExtractor.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

PlanarHomographyExtractor::PlanarHomographyExtractor(
    const PlanarHomographyConfig &config) : m_config(config){
  m_dictionary = cv::aruco::getPredefinedDictionary(config.arucoDictionaryId);

  if (!config.referenceImage.empty()) {
    cv::cvtColor(config.referenceImage,m_refGray, cv::COLOR_BGR2GRAY);
  }

  for (size_t i = 0; i < config.referenceIds.size(); i++) {
    int id = config.referenceIds[i];
    if (i < config.referenceCorners.size() && config.referenceCorners[i].size() == 4) {
      m_refMarkerCorners[id] = config.referenceCorners[i];
    }
    else {
      std::vector<cv::Point2f> defaultCorners = {cv::Point2f(0,0),
                                                  cv::Point2f(1,0),
                                                  cv::Point2f(1,1),
                                                  cv::Point2f(0,1),
      };
      m_refMarkerCorners[id] = defaultCorners;
    }
  }
}

cv::Mat PlanarHomographyExtractor::extract(const cv::Mat &inputImage) {
  if (inputImage.empty()) {
    return cv::Mat();
  }

  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners;
  cv::aruco::detectMarkers(inputImage, m_dictionary, corners, ids);

  if (static_cast<int>(ids.size()) < m_config.minMarkersRequired) {
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //build point correspondences using all four corners of each marker

  std::vector<cv::Point2f> srcPoints;
  std::vector<cv::Point2f> dstPoints;

  for (size_t i = 0; i < ids.size(); i++) {
    int id = ids[i];
    auto it = m_refMarkerCorners.find(id);
    if (it != m_refMarkerCorners.end()) {
      const auto& refCorners = it->second;
      const auto& inputCorners = corners[i];

      for (int j = 0; j < 4; j++) {
        srcPoints.push_back(inputCorners[j]);
        dstPoints.push_back(refCorners[j]);
      }
    }
  }

  if (srcPoints.size() < 4) {
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //compute homography using RANSAC to handle outliers
  cv::Mat H = cv::findHomography(srcPoints, dstPoints, cv::RANSAC,
                                 3.0);
  if (H.empty()) {
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //warp input image to reference view
  cv::Mat warpedColor;
  cv::warpPerspective(inputImage, warpedColor, H,
                      m_config.referenceImage.size(),
                      cv::INTER_LINEAR);

  //extract foreground in warped space
  cv::Mat maskWarped = extractForegroundInWarpedSpace(warpedColor);

  //TODO: morphological cleaning

  cv::Mat maskOriginal;
  cv::Mat H_inv = H.inv();
  cv::warpPerspective(maskWarped, maskOriginal, H_inv,
                      inputImage.size(), cv::INTER_NEAREST);

  return maskOriginal;
}
cv::Mat PlanarHomographyExtractor::extractForegroundInWarpedSpace(
    const cv::Mat &warpedColor) {

  cv::Mat warpedGray;
  cv::cvtColor(warpedColor, warpedGray, cv::COLOR_BGR2GRAY);

  cv::Mat diff;
  cv::absdiff(warpedGray, m_refGray, diff);

  cv::Mat mask;
  cv::threshold(diff, mask, m_config.diffThreshold, 255, cv::THRESH_BINARY);

  if (m_config.useShadowDetection && m_config.referenceImage.empty()) {
    removeShadows(warpedColor,mask);
  }

  return cv::Mat();
}
void PlanarHomographyExtractor::removeShadows(const cv::Mat &warpedColor,
                                              cv::Mat &mask) {
  //TODO remove shadows
}

std::string PlanarHomographyExtractor::name() const {
  return "PlanarHomographyExtractor";
}
