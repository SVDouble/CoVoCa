#pragma once

#include <filesystem>
#include <string>

#include <rfl/Literal.hpp>

#include "covoca/config/Validators.h"

namespace covoca::calibration {

using CalibrationSchema = rfl::Literal<"gs.calibration.config.v1">;
using BoardType = rfl::Literal<"aruco_grid">;
using CornerRefinement = rfl::Literal<"none", "subpix", "contour">;
using covoca::config::NonNegativeDouble;
using covoca::config::PositiveDouble;
using covoca::config::PositiveInt;

/// Filesystem inputs and outputs for one calibration run.
struct CalibrationPaths {
    std::filesystem::path images;
    std::filesystem::path result;
};

/// ArUco GridBoard geometry.
struct ArucoBoardConfig {
    BoardType type;
    std::string dictionary;
    PositiveInt markers_x;
    PositiveInt markers_y;
    PositiveDouble marker_length_m;
    NonNegativeDouble marker_separation_m;
};

/// Marker detector settings.
struct DetectorConfig {
    PositiveInt min_markers_per_frame;
    bool refine_detections = false;
    CornerRefinement corner_refinement;
};

/// Lens calibration flags.
struct CalibrationFlagsConfig {
    bool fix_k4 = false;
    bool fix_k5 = false;
    bool fix_k6 = false;
};

/// Full calibration configuration.
struct CalibrationConfig {
    CalibrationSchema schema;
    std::string name;
    CalibrationPaths paths;
    ArucoBoardConfig board;
    DetectorConfig detector;
    CalibrationFlagsConfig calibration;
};

/**
 * Loads and validates calibration config.
 *
 * Args:
 *   path: YAML or JSON config path.
 *
 * Returns:
 *   Validated config.
 */
CalibrationConfig loadCalibrationConfig(const std::filesystem::path& path);

/**
 * Validates calibration config.
 *
 * Args:
 *   config: Configuration to validate.
 */
void validateCalibrationConfig(const CalibrationConfig& config);

} // namespace covoca::calibration
