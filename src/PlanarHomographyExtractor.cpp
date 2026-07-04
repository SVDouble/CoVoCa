#include "PlanarHomographyExtractor.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <iostream>

PlanarHomographyExtractor::PlanarHomographyExtractor(
    const PlanarHomographyConfig& config)
    : m_config(config) {
  m_dictionary = cv::aruco::getPredefinedDictionary(config.arucoDictionaryId);

  //convert reference image to grayscale
  if (!config.referenceImage.empty()) {
    cv::cvtColor(config.referenceImage, m_refGray, cv::COLOR_BGR2GRAY);
  } else {
    std::cerr << "[Planar] ERROR: Reference image is empty!\n";
  }

  //store marker corners from reference (must be provided in config)
  for (size_t i = 0; i < config.referenceIds.size(); ++i) {
    int id = config.referenceIds[i];
    if (i < config.referenceCorners.size() &&
        config.referenceCorners[i].size() == 4) {
      m_refMarkerCorners[id] = config.referenceCorners[i];
    } else {
      std::cerr << "[Planar] WARNING: Marker " << id << " has invalid corners.\n";
    }
  }

  //precompute the board mask from the reference corners.
  m_boardMask = createBoardMask(m_config.referenceImage.size());

}

cv::Mat PlanarHomographyExtractor::extract(const cv::Mat& inputImage) {
  if (inputImage.empty()) {
    std::cerr << "[Planar] Input image is empty.\n";
    return {};
  }

  //detect markers in the input image
  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners;
  cv::aruco::detectMarkers(inputImage, m_dictionary, corners, ids);
  std::cout << "[Planar] Detected " << ids.size() << " markers.\n";

  if (ids.size() < m_config.minMarkersRequired) {
    std::cerr << "[Planar] Not enough markers.\n";
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //build point correspondences (all four corners of each matched marker)
  std::vector<cv::Point2f> srcPoints, dstPoints;
  for (size_t i = 0; i < ids.size(); ++i) {
    int id = ids[i];
    auto it = m_refMarkerCorners.find(id);
    if (it != m_refMarkerCorners.end()) {
      const auto& refCorners = it->second;
      const auto& inputCorners = corners[i];
      for (int j = 0; j < 4; ++j) {
        srcPoints.push_back(inputCorners[j]);
        dstPoints.push_back(refCorners[j]);
      }
    }
  }

  if (srcPoints.size() < 4) {
    std::cerr << "[Planar] Not enough correspondences.\n";
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //compute homography
  cv::Mat H = cv::findHomography(srcPoints, dstPoints, cv::RANSAC, 3.0);
  if (H.empty()) {
    std::cerr << "[Planar] Homography failed.\n";
    return cv::Mat::zeros(inputImage.size(), CV_8UC1);
  }

  //warp input image to reference view
  cv::Mat warpedColor;
  cv::warpPerspective(inputImage, warpedColor, H,
                      m_config.referenceImage.size(),
                      cv::INTER_LINEAR);

  //foreground segmentation in warped space (simple difference)
  cv::Mat warpedGray;
  cv::cvtColor(warpedColor, warpedGray, cv::COLOR_BGR2GRAY);

  //added blur to help against noise
  cv::Mat warpedBlurred, refBlurred;
  cv::GaussianBlur(warpedGray, warpedBlurred, cv::Size(5, 5), 0);
  cv::GaussianBlur(m_refGray, refBlurred, cv::Size(5, 5), 0);

  //difference where warped is darker than reference
  cv::Mat diff;
  cv::subtract(refBlurred, warpedBlurred, diff);

  cv::Mat maskWarped;
  cv::threshold(diff, maskWarped, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

  cv::Mat markerMask = createMarkerMask(warpedColor.size());
  //erode slightly to avoid cutting into object if it overlaps markers
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3,3));
  cv::erode(markerMask, markerMask, kernel);
  cv::bitwise_and(maskWarped, markerMask, maskWarped);

  //warp mask back to original image coordinates
  cv::Mat maskOriginal;
  cv::Mat H_inv = H.inv();
  cv::warpPerspective(maskWarped, maskOriginal, H_inv,
                      inputImage.size(), cv::INTER_NEAREST);

  return maskOriginal;
}

cv::Mat PlanarHomographyExtractor::createBoardMask(const cv::Size &size) {
  std::vector<cv::Point2f> allCorners;
  for (const auto& pair : m_refMarkerCorners) {
    for (const auto& pt : pair.second) {
      allCorners.push_back(pt);
    }
  }

  if (allCorners.empty()) {
    std::cerr << "[Planar] WARNING: No marker corners for board mask. Using full image.\n";
    return cv::Mat::ones(size, CV_8UC1) * 255;
  }

  //compute convex hull of all marker corners (tightly around the board)
  std::vector<cv::Point2f> hull;
  cv::convexHull(allCorners, hull);
  //expand hull outward by a few pixels to include the white margin
  int expansion = 10;
  cv::Point2f centroid(0,0);
  for (const auto& pt : hull) centroid += pt;
  centroid *= (1.0f / hull.size());

  std::vector<cv::Point> expandedHull;
  for (const auto& pt : hull) {
    cv::Point2f dir = pt - centroid;
    float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
    if (len > 1e-3f) {
      dir *= (float)expansion / len;
    }
    expandedHull.emplace_back(cvRound(pt.x + dir.x), cvRound(pt.y + dir.y));
  }

  cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);
  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{expandedHull}, cv::Scalar(255));
  return mask;
}

cv::Mat PlanarHomographyExtractor::createMarkerMask(const cv::Size &size) {
  cv::Mat markerMask = cv::Mat::ones(size, CV_8U) * 255;

  for (const auto &pair : m_refMarkerCorners) {
    const auto &corners = pair.second;
    std::vector<cv::Point> pts;
    pts.reserve(corners.size());
    for (const auto &pt : corners) {
      pts.emplace_back(cvRound(pt.x), cvRound(pt.y));
    }
    cv::fillPoly(markerMask,
                 std::vector<std::vector<cv::Point>>{pts},
                 cv::Scalar(0));
  }
  return markerMask;
}

std::string PlanarHomographyExtractor::name() const {
  return "PlanarHomographyExtractor";
}
