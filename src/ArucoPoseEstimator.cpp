#include "ArucoPoseEstimator.h"

#include <iostream>
#include <opencv2/calib3d.hpp>    // For cv::Rodrigues matrix
#include <opencv2/core/eigen.hpp> // For cv::cv2eigen (OpenCV -> Eigen)

ArucoPoseEstimator::ArucoPoseEstimator(int _markers_x, int _markers_y,
                                       float _marker_length,
                                       float _marker_separation,
                                       int _dict_name) {
  // 1. Load Dictionary
  m_dictionary = cv::aruco::getPredefinedDictionary(_dict_name);

  // 2. Create the grid board object with the specified parameters
  m_board = cv::makePtr<cv::aruco::GridBoard>(cv::Size(_markers_x, _markers_y),
                                              _marker_length,
                                              _marker_separation, m_dictionary);

  // 3. Initialize the detector parameters
  m_detector_params.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;

  // 4. Calculate board center offsets mathematically (objPoints is inaccessible
  // in this OpenCV version)
  m_board_offset_x =
      (_markers_x * _marker_length + (_markers_x - 1) * _marker_separation) /
      2.0f;
  m_board_offset_y =
      (_markers_y * _marker_length + (_markers_y - 1) * _marker_separation) /
      2.0f;
}

void ArucoPoseEstimator::setIntrinsics(const cv::Mat &_camera_matrix,
                                       const cv::Mat &_dist_coeffs) {
  m_camera_matrix = _camera_matrix.clone();
  m_dist_coeffs = _dist_coeffs.clone();
}

bool ArucoPoseEstimator::estimateCameraPose(const cv::Mat &_image,
                                            Camera &_out_camera) {

  if (m_camera_matrix.empty() || m_dist_coeffs.empty()) {
    std::cerr << "[ArucoPoseEstimator] error: no intrinsics" << std::endl;
    return false;
  }

  std::vector<int> marker_ids;
  std::vector<std::vector<cv::Point2f>> marker_corners, rejected_candidates;

  // Detect markers in the input image
  cv::aruco::ArucoDetector detector(m_dictionary, m_detector_params);
  detector.detectMarkers(_image, marker_corners, marker_ids,
                         rejected_candidates);

  // No marker then false
  if (marker_ids.empty()) {
    return false;
  }

  // Refine detected markers (helps with partial occlusion)
  detector.refineDetectedMarkers(_image, *m_board, marker_corners, marker_ids,
                                 rejected_candidates, m_camera_matrix,
                                 m_dist_coeffs);

  // compute the pose of the board
  cv::Vec3d rvec, tvec;

  // extract the pose of the board
  int valid_markers =
      cv::aruco::estimatePoseBoard(marker_corners, marker_ids, m_board,
                                   m_camera_matrix, m_dist_coeffs, rvec, tvec);

  if (valid_markers > 0) {

    // from OpenCV's Rodrigues output to Eigen
    cv::Mat R_cv;
    cv::Rodrigues(rvec, R_cv);

    Eigen::Matrix3d R_eigen;
    Eigen::Vector3d t_eigen;
    Eigen::Matrix3d K_eigen;

    cv::cv2eigen(R_cv, R_eigen);
    cv::cv2eigen(tvec, t_eigen);
    cv::cv2eigen(m_camera_matrix, K_eigen);

    // Shift origin to the center of the board
    Eigen::Vector3d offset(m_board_offset_x, m_board_offset_y, 0.0);
    t_eigen = t_eigen + R_eigen * offset;

    // Convert the board coordinate system so that Z points UP from the board
    // instead of DOWN. OpenCV's GridBoard default: X right, Y down, Z into the
    // plane. Multiply by 180 degree rotation around X to flip Y and Z.
    Eigen::Matrix3d Rx180;
    Rx180 << 1, 0, 0, 0, -1, 0, 0, 0, -1;
    R_eigen = R_eigen * Rx180;

    _out_camera.setIntrinsics(K_eigen);
    _out_camera.setRotation(R_eigen);
    _out_camera.setTranslation(t_eigen);

    return true;
  }

  return false;
}
