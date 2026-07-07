#include "ArucoPoseEstimator.h"
#include <Eigen/Dense>
#include <iostream>
#include <opencv2/core/eigen.hpp>
#include <opencv2/opencv.hpp>
int main(int argc, char **argv) {
  // 1. Set up the Aruco board parameters based on the cat dataset
  // calibration_result.yaml
  int markers_x = 4;
  int markers_y = 5;
  float marker_length = 0.0375f;
  float marker_separation = 0.005f;
  int dict_name = cv::aruco::DICT_6X6_1000;

  ArucoPoseEstimator estimator(markers_x, markers_y, marker_length,
                               marker_separation, dict_name);

  // 2. Set up the camera intrinsics
  cv::Mat camera_matrix =
      (cv::Mat_<double>(3, 3) << 870.195780, 0.000000, 634.411075, 0.000000,
       862.140009, 499.328103, 0.000000, 0.000000, 1.000000);

  cv::Mat dist_coeffs = (cv::Mat_<double>(5, 1) << 0.170929, -0.949321,
                         -0.020620, 0.000191, 1.597958);

  estimator.setIntrinsics(camera_matrix, dist_coeffs);

  // 3. Load an image from the dataset
  std::string image_path =
      "datasets/cat/images/photo_13_2026-06-12_21-24-45.jpg";
  cv::Mat image = cv::imread(image_path);
  if (image.empty()) {
    std::cerr << "Failed to load image: " << image_path << std::endl;
    return 1;
  }

  // 4. Estimate camera pose
  Camera out_camera;
  bool success = estimator.estimateCameraPose(image, out_camera);

  if (success) {
    std::cout << "Successfully estimated camera pose!" << std::endl;

    // --- Draw the result for visual verification ---
    cv::Mat result_image = image.clone();

    // Convert Eigen rotation matrix back to cv::Vec3d (rvec)
    Eigen::Matrix3d R_eigen = out_camera.getRotation();
    cv::Mat R_cv;
    cv::eigen2cv(R_eigen, R_cv);
    cv::Vec3d rvec;
    cv::Rodrigues(R_cv, rvec);

    // Convert Eigen translation vector back to cv::Vec3d (tvec)
    Eigen::Vector3d t_eigen = out_camera.getTranslation();
    cv::Vec3d tvec(t_eigen.x(), t_eigen.y(), t_eigen.z());

    // --- Optional: Draw individual markers to match standard tutorials ---
    std::vector<int> marker_ids;
    std::vector<std::vector<cv::Point2f>> marker_corners, rejected;
    cv::aruco::Dictionary dict = cv::aruco::getPredefinedDictionary(dict_name);
    cv::aruco::ArucoDetector(dict).detectMarkers(image, marker_corners,
                                                 marker_ids, rejected);

    if (!marker_ids.empty()) {
      cv::aruco::drawDetectedMarkers(result_image, marker_corners, marker_ids);
      std::vector<cv::Vec3d> rvecs, tvecs;
      cv::aruco::estimatePoseSingleMarkers(marker_corners, marker_length,
                                           camera_matrix, dist_coeffs, rvecs,
                                           tvecs);
      for (size_t i = 0; i < rvecs.size(); ++i) {
        // Draw axes on each individual marker (length = half of marker length)
        cv::drawFrameAxes(result_image, camera_matrix, dist_coeffs, rvecs[i],
                          tvecs[i], marker_length * 0.8f);
      }
    }

    // Draw coordinate axes for the WHOLE board (Global World Coordinate System
    // for Voxel Carving) This is the most important one! It shows the origin
    // (0,0,0) of your 3D reconstruction.
    cv::drawFrameAxes(result_image, camera_matrix, dist_coeffs, rvec, tvec,
                      0.12f);

    // Save and output the image
    std::string output_path = "aruco_pose_verification.jpg";
    cv::imwrite(output_path, result_image);
    std::cout << "Saved verification image to: " << output_path << std::endl;
    std::cout << "Rotation Matrix:\n" << R_eigen << std::endl;
    std::cout << "Translation Vector:\n" << t_eigen << std::endl;
  } else {
    std::cerr << "Failed to estimate camera pose." << std::endl;
    return 1;
  }

  return 0;
}
