#include "gsplat/calibration/CalibrationResult.h"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <rfl/Literal.hpp>
#include <rfl/Validator.hpp>
#include <rfl/comparisons.hpp>
#include <rfl/yaml.hpp>

#include "gsplat/config/ConfigIO.h"

namespace gs::calibration {
namespace {

constexpr const char* kCalibrationResultSchema = "gs.calibration.result.v1";

using ResultSchema = rfl::Literal<"gs.calibration.result.v1">;
using BoardType = rfl::Literal<"aruco_grid">;
using PositiveInt = rfl::Validator<int, rfl::ExclusiveMinimum<0>>;
using PositiveDouble = rfl::Validator<double, rfl::ExclusiveMinimum<0>>;
using NonNegativeDouble = rfl::Validator<double, rfl::Minimum<0>>;
using Vector3Values = std::array<double, 3>;

template <int Rows, int Cols>
using MatrixRows = std::array<std::array<double, static_cast<std::size_t>(Cols)>, static_cast<std::size_t>(Rows)>;

using Matrix3Rows = MatrixRows<3, 3>;
using Matrix4Rows = MatrixRows<4, 4>;

struct SourceModel {
    std::filesystem::path config;
    std::filesystem::path image_path;
    PositiveInt input_image_count;
    PositiveInt accepted_frame_count;
};

struct ImageSizeModel {
    PositiveInt width;
    PositiveInt height;
};

struct ArucoBoardModel {
    BoardType type;
    std::string dictionary;
    PositiveInt markers_x;
    PositiveInt markers_y;
    PositiveDouble marker_length_m;
    NonNegativeDouble marker_separation_m;
};

struct CameraModelYaml {
    Matrix3Rows matrix;
    std::vector<double> distortion_coefficients;
};

struct FrameModel {
    std::filesystem::path image;
    PositiveInt detected_marker_count;
    PositiveInt matched_corner_count;
    NonNegativeDouble mean_reprojection_error_px;
    std::vector<int> detected_ids;
    Vector3Values rvec_board_to_camera;
    Vector3Values tvec_board_to_camera_m;
    Matrix3Rows rotation_board_to_camera;
    Matrix4Rows transform_board_to_camera;
    Vector3Values camera_center_board_m;
};

struct CalibrationResultModel {
    ResultSchema schema;
    std::string name;
    SourceModel source;
    ImageSizeModel image_size;
    NonNegativeDouble rms_reprojection_error_px;
    ArucoBoardModel board;
    CameraModelYaml camera;
    std::vector<FrameModel> frames;
};

template <int Rows, int Cols> MatrixRows<Rows, Cols> toRows(const Eigen::Matrix<double, Rows, Cols>& values) {
    MatrixRows<Rows, Cols> rows{};
    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = values(row, col);
        }
    }
    return rows;
}

template <int Rows, int Cols> Eigen::Matrix<double, Rows, Cols> fromRows(const MatrixRows<Rows, Cols>& rows) {
    Eigen::Matrix<double, Rows, Cols> values;
    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Cols; ++col) {
            values(row, col) = rows[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
        }
    }
    return values;
}

Vector3 toVector3(const Vector3Values& values) {
    return Eigen::Map<const Vector3>(values.data());
}

Vector3Values toVector3Values(const Vector3& values) {
    return {values.x(), values.y(), values.z()};
}

VectorX toVectorX(const std::vector<double>& values) {
    if (values.empty()) {
        return {};
    }
    return Eigen::Map<const VectorX>(values.data(), static_cast<Eigen::Index>(values.size()));
}

std::vector<double> toStdVector(const VectorX& values) {
    if (values.size() == 0) {
        return {};
    }
    return {values.data(), values.data() + values.size()};
}

ArucoBoardConfig toBoard(const ArucoBoardModel& model) {
    return {
        .type = model.type.name(),
        .dictionary = model.dictionary,
        .markersX = model.markers_x.value(),
        .markersY = model.markers_y.value(),
        .markerLengthMeters = model.marker_length_m.value(),
        .markerSeparationMeters = model.marker_separation_m.value(),
    };
}

ArucoBoardModel toBoardModel(const ArucoBoardConfig& board) {
    return {
        .type = BoardType{board.type},
        .dictionary = board.dictionary,
        .markers_x = PositiveInt{board.markersX},
        .markers_y = PositiveInt{board.markersY},
        .marker_length_m = PositiveDouble{board.markerLengthMeters},
        .marker_separation_m = NonNegativeDouble{board.markerSeparationMeters},
    };
}

CalibrationResult toCalibrationResult(const CalibrationResultModel& model) {
    std::vector<FrameCalibrationResult> frames;
    frames.reserve(model.frames.size());
    for (const FrameModel& frame : model.frames) {
        frames.push_back({
            .image = frame.image,
            .detectedMarkerCount = frame.detected_marker_count.value(),
            .matchedCornerCount = frame.matched_corner_count.value(),
            .meanReprojectionErrorPixels = frame.mean_reprojection_error_px.value(),
            .detectedIds = frame.detected_ids,
            .rvecBoardToCamera = toVector3(frame.rvec_board_to_camera),
            .tvecBoardToCameraMeters = toVector3(frame.tvec_board_to_camera_m),
            .rotationBoardToCamera = fromRows<3, 3>(frame.rotation_board_to_camera),
            .transformBoardToCamera = fromRows<4, 4>(frame.transform_board_to_camera),
            .cameraCenterBoardMeters = toVector3(frame.camera_center_board_m),
        });
    }

    return {
        .schema = model.schema.name(),
        .name = model.name,
        .sourceConfig = model.source.config,
        .imagePath = model.source.image_path,
        .inputImageCount = model.source.input_image_count.value(),
        .acceptedFrameCount = model.source.accepted_frame_count.value(),
        .imageSize = {.width = model.image_size.width.value(), .height = model.image_size.height.value()},
        .rmsReprojectionErrorPixels = model.rms_reprojection_error_px.value(),
        .board = toBoard(model.board),
        .camera = {.matrix = fromRows<3, 3>(model.camera.matrix),
                   .distortionCoefficients = toVectorX(model.camera.distortion_coefficients)},
        .frames = std::move(frames),
    };
}

CalibrationResultModel toCalibrationResultModel(const CalibrationResult& result) {
    std::vector<FrameModel> frames;
    frames.reserve(result.frames.size());
    for (const FrameCalibrationResult& frame : result.frames) {
        frames.push_back({
            .image = frame.image,
            .detected_marker_count = PositiveInt{frame.detectedMarkerCount},
            .matched_corner_count = PositiveInt{frame.matchedCornerCount},
            .mean_reprojection_error_px = NonNegativeDouble{frame.meanReprojectionErrorPixels},
            .detected_ids = frame.detectedIds,
            .rvec_board_to_camera = toVector3Values(frame.rvecBoardToCamera),
            .tvec_board_to_camera_m = toVector3Values(frame.tvecBoardToCameraMeters),
            .rotation_board_to_camera = toRows<3, 3>(frame.rotationBoardToCamera),
            .transform_board_to_camera = toRows<4, 4>(frame.transformBoardToCamera),
            .camera_center_board_m = toVector3Values(frame.cameraCenterBoardMeters),
        });
    }

    return {
        .schema = ResultSchema{result.schema},
        .name = result.name,
        .source = {.config = result.sourceConfig,
                   .image_path = result.imagePath,
                   .input_image_count = PositiveInt{result.inputImageCount},
                   .accepted_frame_count = PositiveInt{result.acceptedFrameCount}},
        .image_size = {.width = PositiveInt{result.imageSize.width}, .height = PositiveInt{result.imageSize.height}},
        .rms_reprojection_error_px = NonNegativeDouble{result.rmsReprojectionErrorPixels},
        .board = toBoardModel(result.board),
        .camera = {.matrix = toRows<3, 3>(result.camera.matrix),
                   .distortion_coefficients = toStdVector(result.camera.distortionCoefficients)},
        .frames = std::move(frames),
    };
}

void validateCalibrationResult(const CalibrationResult& result) {
    if (result.schema != kCalibrationResultSchema) {
        throw std::runtime_error("unsupported calibration result schema: " + result.schema);
    }
    if (result.name.empty()) {
        throw std::runtime_error("result field must not be empty: name");
    }
    if (result.imagePath.empty()) {
        throw std::runtime_error("result field must not be empty: source.image_path");
    }
    if (result.board.type != "aruco_grid") {
        throw std::runtime_error("unsupported result board.type: " + result.board.type);
    }
    if (result.board.dictionary.empty()) {
        throw std::runtime_error("result field must not be empty: board.dictionary");
    }
    if (result.inputImageCount <= 0) {
        throw std::runtime_error("result field must be positive: source.input_image_count");
    }
    if (result.acceptedFrameCount <= 0) {
        throw std::runtime_error("result field must be positive: source.accepted_frame_count");
    }
    if (result.imageSize.width <= 0 || result.imageSize.height <= 0) {
        throw std::runtime_error("result image size must be positive");
    }
    if (result.camera.distortionCoefficients.size() == 0) {
        throw std::runtime_error("result field must not be empty: camera.distortion_coefficients");
    }
    if (std::cmp_not_equal(result.frames.size(), result.acceptedFrameCount)) {
        throw std::runtime_error("result accepted_frame_count does not match frames size");
    }
}

} // namespace

void saveCalibrationResult(const std::filesystem::path& path, const CalibrationResult& result) {
    validateCalibrationResult(result);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    auto status = rfl::yaml::save(path.string(), toCalibrationResultModel(result));
    if (!status) {
        throw std::runtime_error("failed to write calibration result " + path.string() + ": " + status.error().what());
    }
}

CalibrationResult loadCalibrationResult(const std::filesystem::path& path) {
    CalibrationResult result =
        toCalibrationResult(gs::config::loadTypedConfig<CalibrationResultModel>(path, "calibration result"));
    validateCalibrationResult(result);
    return result;
}

} // namespace gs::calibration
