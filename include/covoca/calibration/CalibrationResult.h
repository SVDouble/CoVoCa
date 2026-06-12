#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "covoca/calibration/CalibrationConfig.h"
#include "covoca/config/EigenReflectors.h"

namespace covoca::calibration {

using Vector3 = Eigen::Vector3d;
using VectorX = Eigen::VectorXd;
using Matrix3 = Eigen::Matrix3d;
using Matrix4 = Eigen::Matrix4d;

using ResultSchema = rfl::Literal<"gs.calibration.result.v1">;

/// Image dimensions.
struct ImageSize {
    PositiveInt width;
    PositiveInt height;
};

/// Pinhole camera model.
struct CameraModel {
    Matrix3 matrix;
    VectorX distortion_coefficients;
};

/// Source images and frame counts for a calibration run.
struct CalibrationSource {
    std::filesystem::path config;
    std::filesystem::path image_path;
    PositiveInt input_image_count;
    PositiveInt accepted_frame_count;
};

/// Per-frame pose and calibration diagnostics.
struct FrameCalibrationResult {
    std::filesystem::path image;
    PositiveInt detected_marker_count;
    PositiveInt matched_corner_count;
    NonNegativeDouble mean_reprojection_error_px;
    std::vector<int> detected_ids;
    Vector3 rvec_board_to_camera;
    Vector3 tvec_board_to_camera_m;
    Matrix3 rotation_board_to_camera;
    Matrix4 transform_board_to_camera;
    Vector3 camera_center_board_m;
};

/// Calibration output schema.
struct CalibrationResult {
    ResultSchema schema;
    std::string name;
    CalibrationSource source;
    ImageSize image_size;
    NonNegativeDouble rms_reprojection_error_px;
    ArucoBoardConfig board;
    CameraModel camera;
    std::vector<FrameCalibrationResult> frames;
};

/**
 * Writes calibration result YAML.
 *
 * Args:
 *   path: Destination YAML file.
 *   result: Result to write.
 */
void saveCalibrationResult(const std::filesystem::path& path, const CalibrationResult& result);

/**
 * Loads and validates calibration result.
 *
 * Args:
 *   path: YAML or JSON result path.
 *
 * Returns:
 *   Validated result.
 */
CalibrationResult loadCalibrationResult(const std::filesystem::path& path);

} // namespace covoca::calibration
