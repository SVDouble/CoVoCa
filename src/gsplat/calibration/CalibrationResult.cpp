#include "gsplat/calibration/CalibrationResult.h"

#include <filesystem>
#include <stdexcept>
#include <utility>

#include <rfl/yaml.hpp>

#include "gsplat/config/ConfigIO.h"

namespace gs::calibration {
namespace {

void validateCalibrationResult(const CalibrationResult& result) {
    if (result.name.empty()) {
        throw std::runtime_error("result field must not be empty: name");
    }
    if (result.source.image_path.empty()) {
        throw std::runtime_error("result field must not be empty: source.image_path");
    }
    if (result.board.dictionary.empty()) {
        throw std::runtime_error("result field must not be empty: board.dictionary");
    }
    if (result.camera.distortion_coefficients.size() == 0) {
        throw std::runtime_error("result field must not be empty: camera.distortion_coefficients");
    }
    if (std::cmp_not_equal(result.frames.size(), result.source.accepted_frame_count.value())) {
        throw std::runtime_error("result accepted_frame_count does not match frames size");
    }
}

} // namespace

void saveCalibrationResult(const std::filesystem::path& path, const CalibrationResult& result) {
    validateCalibrationResult(result);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    auto status = rfl::yaml::save(path.string(), result);
    if (!status) {
        throw std::runtime_error("failed to write calibration result " + path.string() + ": " + status.error().what());
    }
}

CalibrationResult loadCalibrationResult(const std::filesystem::path& path) {
    auto result = gs::config::loadTypedConfig<CalibrationResult>(path, "calibration result");
    validateCalibrationResult(result);
    return result;
}

} // namespace gs::calibration
