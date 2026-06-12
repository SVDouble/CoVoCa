#pragma once

#include <filesystem>

#include "gsplat/calibration/CalibrationConfig.h"
#include "gsplat/calibration/CalibrationResult.h"

namespace gs::calibration {

/**
 * @brief Calibrates intrinsics and board-to-camera poses.
 *
 * Args:
 *   config: Calibration config.
 *   sourceConfigPath: Config path stored in the result.
 *
 * Returns:
 *   Calibration result.
 */
CalibrationResult runArucoCalibration(const CalibrationConfig& config, const std::filesystem::path& sourceConfigPath);

/**
 * @brief Writes coordinate-axis overlays.
 *
 * Args:
 *   result: Calibration result.
 *   outputDir: Output directory.
 *   axisLengthMeters: Axis length in meters.
 *
 * Returns:
 *   Number of images written.
 */
int writeCoordinateSystemVisualizations(const CalibrationResult& result, const std::filesystem::path& outputDir,
                                        double axisLengthMeters);

} // namespace gs::calibration
