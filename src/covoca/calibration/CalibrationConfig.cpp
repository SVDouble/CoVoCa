#include "covoca/calibration/CalibrationConfig.h"

#include <stdexcept>

#include "covoca/config/ConfigIO.h"

namespace covoca::calibration {

CalibrationConfig loadCalibrationConfig(const std::filesystem::path& path) {
    auto config = covoca::config::loadTypedConfig<CalibrationConfig>(path, "calibration config");
    validateCalibrationConfig(config);
    return config;
}

void validateCalibrationConfig(const CalibrationConfig& config) {
    if (config.name.empty()) {
        throw std::runtime_error("config field must not be empty: name");
    }
    if (config.paths.images.empty()) {
        throw std::runtime_error("config field must not be empty: paths.images");
    }
    if (config.paths.result.empty()) {
        throw std::runtime_error("config field must not be empty: paths.result");
    }
    if (config.board.dictionary.empty()) {
        throw std::runtime_error("config field must not be empty: board.dictionary");
    }
}

} // namespace covoca::calibration
