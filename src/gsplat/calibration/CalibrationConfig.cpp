#include "gsplat/calibration/CalibrationConfig.h"

#include <stdexcept>
#include <string>

#include <rfl/Literal.hpp>
#include <rfl/Validator.hpp>
#include <rfl/comparisons.hpp>

#include "gsplat/config/ConfigIO.h"

namespace gs::calibration {
namespace {

constexpr const char* kCalibrationSchema = "gs.calibration.config.v1";
constexpr const char* kArucoGridBoard = "aruco_grid";

using CalibrationSchema = rfl::Literal<"gs.calibration.config.v1">;
using BoardType = rfl::Literal<"aruco_grid">;
using CornerRefinement = rfl::Literal<"none", "subpix", "contour">;
using PositiveInt = rfl::Validator<int, rfl::ExclusiveMinimum<0>>;
using PositiveDouble = rfl::Validator<double, rfl::ExclusiveMinimum<0>>;
using NonNegativeDouble = rfl::Validator<double, rfl::Minimum<0>>;

struct CalibrationPathsModel {
    std::filesystem::path images;
    std::filesystem::path result;
};

struct ArucoBoardConfigModel {
    BoardType type;
    std::string dictionary;
    PositiveInt markers_x;
    PositiveInt markers_y;
    PositiveDouble marker_length_m;
    NonNegativeDouble marker_separation_m;
};

struct DetectorConfigModel {
    PositiveInt min_markers_per_frame;
    bool refine_detections = false;
    CornerRefinement corner_refinement;
};

struct CalibrationFlagsConfigModel {
    bool fix_k4 = false;
    bool fix_k5 = false;
    bool fix_k6 = false;
};

struct CalibrationConfigModel {
    CalibrationSchema schema;
    std::string name;
    CalibrationPathsModel paths;
    ArucoBoardConfigModel board;
    DetectorConfigModel detector;
    CalibrationFlagsConfigModel calibration;
};

CalibrationConfig toCalibrationConfig(const CalibrationConfigModel& model) {
    return {
        .schema = model.schema.name(),
        .name = model.name,
        .paths =
            {
                .images = model.paths.images,
                .result = model.paths.result,
            },
        .board =
            {
                .type = model.board.type.name(),
                .dictionary = model.board.dictionary,
                .markersX = model.board.markers_x.value(),
                .markersY = model.board.markers_y.value(),
                .markerLengthMeters = model.board.marker_length_m.value(),
                .markerSeparationMeters = model.board.marker_separation_m.value(),
            },
        .detector =
            {
                .minMarkersPerFrame = model.detector.min_markers_per_frame.value(),
                .refineDetections = model.detector.refine_detections,
                .cornerRefinement = model.detector.corner_refinement.name(),
            },
        .calibration =
            {
                .fixK4 = model.calibration.fix_k4,
                .fixK5 = model.calibration.fix_k5,
                .fixK6 = model.calibration.fix_k6,
            },
    };
}

void requirePositive(int value, const std::string& field) {
    if (value <= 0) {
        throw std::runtime_error("config field must be positive: " + field);
    }
}

void requirePositive(double value, const std::string& field) {
    if (value <= 0.0) {
        throw std::runtime_error("config field must be positive: " + field);
    }
}

} // namespace

CalibrationConfig loadCalibrationConfig(const std::filesystem::path& path) {
    CalibrationConfig config =
        toCalibrationConfig(gs::config::loadTypedConfig<CalibrationConfigModel>(path, "calibration config"));
    validateCalibrationConfig(config);
    return config;
}

void validateCalibrationConfig(const CalibrationConfig& config) {
    if (config.schema != kCalibrationSchema) {
        throw std::runtime_error("unsupported calibration config schema: " + config.schema);
    }
    if (config.name.empty()) {
        throw std::runtime_error("config field must not be empty: name");
    }
    if (config.paths.images.empty()) {
        throw std::runtime_error("config field must not be empty: paths.images");
    }
    if (config.paths.result.empty()) {
        throw std::runtime_error("config field must not be empty: paths.result");
    }
    if (config.board.type != kArucoGridBoard) {
        throw std::runtime_error("unsupported board.type: " + config.board.type);
    }
    if (config.board.dictionary.empty()) {
        throw std::runtime_error("config field must not be empty: board.dictionary");
    }
    requirePositive(config.board.markersX, "board.markers_x");
    requirePositive(config.board.markersY, "board.markers_y");
    requirePositive(config.board.markerLengthMeters, "board.marker_length_m");
    if (config.board.markerSeparationMeters < 0.0) {
        throw std::runtime_error("config field must not be negative: board.marker_separation_m");
    }
    requirePositive(config.detector.minMarkersPerFrame, "detector.min_markers_per_frame");
    if (config.detector.cornerRefinement != "none" && config.detector.cornerRefinement != "subpix" &&
        config.detector.cornerRefinement != "contour") {
        throw std::runtime_error("unsupported detector.corner_refinement: " + config.detector.cornerRefinement);
    }
}

} // namespace gs::calibration
