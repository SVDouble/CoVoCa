#pragma once

#include <Eigen/Dense>
#include <opencv2/aruco.hpp>
#include <opencv2/opencv.hpp>

#include "Camera.h"

class ArucoPoseEstimator {
public:
  // ArUco board constructor
  //_markers_x: Number of markers along the X-axis
  //_markers_y: Number of markers along the Y-axis

  //_marker_length: Length of each markers side (m)
  //_marker_separation: Distance between markers (m)
  //_dict_name: Predefined ArUco dictionary to use for marker generation and
  //detection

  ArucoPoseEstimator(int _markers_x, int _markers_y, float _marker_length,
                     float _marker_separation, int _dict_name);

  // Set camera intrinsics and distortion coefficients
  void setIntrinsics(const cv::Mat &_camera_matrix,
                     const cv::Mat &_dist_coeffs);

  // Estimate camera pose from an input image containing the ArUco board
  bool estimateCameraPose(const cv::Mat &_image, Camera &_out_camera);

private:
  cv::aruco::Dictionary m_dictionary;
  cv::Ptr<cv::aruco::GridBoard> m_board;
  cv::aruco::DetectorParameters m_detector_params;

  cv::Mat m_camera_matrix;
  cv::Mat m_dist_coeffs;

  float m_board_offset_x;
  float m_board_offset_y;
};
