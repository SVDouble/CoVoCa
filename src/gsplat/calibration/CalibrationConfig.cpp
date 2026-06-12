#include "gsplat/calibration/CalibrationConfig.h"

#include <stdexcept>

#include "gsplat/config/ConfigIO.h"

namespace gs::calibration {

CalibrationConfig loadCalibrationConfig(const std::filesystem::path& path) {
    auto config = gs::config::loadTypedConfig<CalibrationConfig>(path, "calibration config");
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

} // namespace gs::calibration
