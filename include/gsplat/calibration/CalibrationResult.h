#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "gsplat/calibration/CalibrationConfig.h"

namespace gs::calibration {

using Vector3 = Eigen::Vector3d;
using VectorX = Eigen::VectorXd;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;

/// Image dimensions.
struct ImageSize {
    int width = 0;
    int height = 0;
};

/// Pinhole camera model.
struct CameraModel {
    Matrix3 matrix = Matrix3::Zero();
    VectorX distortionCoefficients;
};

/// Per-frame pose and calibration diagnostics.
struct FrameCalibrationResult {
    std::filesystem::path image;
    int detectedMarkerCount = 0;
    int matchedCornerCount = 0;
    double meanReprojectionErrorPixels = 0.0;
    std::vector<int> detectedIds;
    Vector3 rvecBoardToCamera = Vector3::Zero();
    Vector3 tvecBoardToCameraMeters = Vector3::Zero();
    Matrix3 rotationBoardToCamera = Matrix3::Zero();
    Matrix4 transformBoardToCamera = Matrix4::Identity();
    Vector3 cameraCenterBoardMeters = Vector3::Zero();
};

/// Calibration output schema.
struct CalibrationResult {
    std::string schema;
    std::string name;
    std::filesystem::path sourceConfig;
    std::filesystem::path imagePath;
    int inputImageCount = 0;
    int acceptedFrameCount = 0;
    ImageSize imageSize;
    double rmsReprojectionErrorPixels = 0.0;
    ArucoBoardConfig board;
    CameraModel camera;
    std::vector<FrameCalibrationResult> frames;
};

/**
 * @brief Writes calibration result YAML.
 *
 * Args:
 *   path: Destination YAML file.
 *   result: Result to write.
 */
void saveCalibrationResult(const std::filesystem::path& path, const CalibrationResult& result);

/**
 * @brief Loads and validates calibration result.
 *
 * Args:
 *   path: YAML or JSON result path.
 *
 * Returns:
 *   Validated result.
 */
CalibrationResult loadCalibrationResult(const std::filesystem::path& path);

} // namespace gs::calibration
