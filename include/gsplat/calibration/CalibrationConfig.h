#pragma once

#include <filesystem>
#include <string>

namespace gs::calibration {

/// Filesystem inputs and outputs for one calibration run.
struct CalibrationPaths {
    std::filesystem::path images;
    std::filesystem::path result;
};

/// ArUco GridBoard geometry.
struct ArucoBoardConfig {
    std::string type;
    std::string dictionary;
    int markersX = 0;
    int markersY = 0;
    double markerLengthMeters = 0.0;
    double markerSeparationMeters = 0.0;
};

/// Marker detector settings.
struct DetectorConfig {
    int minMarkersPerFrame = 0;
    bool refineDetections = false;
    std::string cornerRefinement;
};

/// Lens calibration flags.
struct CalibrationFlagsConfig {
    bool fixK4 = false;
    bool fixK5 = false;
    bool fixK6 = false;
};

/// Full calibration configuration.
struct CalibrationConfig {
    std::string schema;
    std::string name;
    CalibrationPaths paths;
    ArucoBoardConfig board;
    DetectorConfig detector;
    CalibrationFlagsConfig calibration;
};

/**
 * @brief Loads and validates calibration config.
 *
 * Args:
 *   path: YAML or JSON config path.
 *
 * Returns:
 *   Validated config.
 */
CalibrationConfig loadCalibrationConfig(const std::filesystem::path& path);

/**
 * @brief Validates calibration config.
 *
 * Args:
 *   config: Configuration to validate.
 */
void validateCalibrationConfig(const CalibrationConfig& config);

} // namespace gs::calibration
