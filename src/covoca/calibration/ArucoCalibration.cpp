#include "covoca/calibration/ArucoCalibration.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <Eigen/Dense>
#include <opencv2/aruco.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/imgcodecs.hpp>
#include <spdlog/spdlog.h>

namespace covoca::calibration {
namespace fs = std::filesystem;

namespace {

std::string toLower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string normalizeDictionaryName(std::string name) {
    if (name.starts_with("cv::aruco::")) {
        name.erase(0, std::string("cv::aruco::").size());
    }
    return name;
}

/**
 * @brief Looks up an OpenCV ArUco dictionary by config name.
 *
 * Args:
 *   rawName: Dictionary name, optionally prefixed with `cv::aruco::`.
 *
 * Returns:
 *   OpenCV predefined dictionary type when supported.
 */
std::optional<cv::aruco::PredefinedDictionaryType> dictionaryTypeFromName(const std::string& rawName) {
    static const std::unordered_map<std::string, cv::aruco::PredefinedDictionaryType> dictionaries = {
        {"DICT_4X4_50", cv::aruco::DICT_4X4_50},
        {"DICT_4X4_100", cv::aruco::DICT_4X4_100},
        {"DICT_4X4_250", cv::aruco::DICT_4X4_250},
        {"DICT_4X4_1000", cv::aruco::DICT_4X4_1000},
        {"DICT_5X5_50", cv::aruco::DICT_5X5_50},
        {"DICT_5X5_100", cv::aruco::DICT_5X5_100},
        {"DICT_5X5_250", cv::aruco::DICT_5X5_250},
        {"DICT_5X5_1000", cv::aruco::DICT_5X5_1000},
        {"DICT_6X6_50", cv::aruco::DICT_6X6_50},
        {"DICT_6X6_100", cv::aruco::DICT_6X6_100},
        {"DICT_6X6_250", cv::aruco::DICT_6X6_250},
        {"DICT_6X6_1000", cv::aruco::DICT_6X6_1000},
        {"DICT_7X7_50", cv::aruco::DICT_7X7_50},
        {"DICT_7X7_100", cv::aruco::DICT_7X7_100},
        {"DICT_7X7_250", cv::aruco::DICT_7X7_250},
        {"DICT_7X7_1000", cv::aruco::DICT_7X7_1000},
        {"DICT_ARUCO_ORIGINAL", cv::aruco::DICT_ARUCO_ORIGINAL},
        {"DICT_APRILTAG_16h5", cv::aruco::DICT_APRILTAG_16h5},
        {"DICT_APRILTAG_25h9", cv::aruco::DICT_APRILTAG_25h9},
        {"DICT_APRILTAG_36h10", cv::aruco::DICT_APRILTAG_36h10},
        {"DICT_APRILTAG_36h11", cv::aruco::DICT_APRILTAG_36h11},
    };

    const auto name = normalizeDictionaryName(rawName);
    if (const auto it = dictionaries.find(name); it != dictionaries.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Converts the configured corner refinement name to OpenCV flags.
 *
 * Args:
 *   name: Config value for `detector.corner_refinement`.
 *
 * Returns:
 *   OpenCV ArUco corner refinement enum value.
 */
int cornerRefinementMethodFromName(const std::string& name) {
    if (name == "none") {
        return cv::aruco::CORNER_REFINE_NONE;
    }
    if (name == "subpix") {
        return cv::aruco::CORNER_REFINE_SUBPIX;
    }
    if (name == "contour") {
        return cv::aruco::CORNER_REFINE_CONTOUR;
    }
    throw std::runtime_error("unsupported detector.corner_refinement: " + name);
}

/**
 * @brief Converts calibration config booleans to OpenCV calibration flags.
 *
 * Args:
 *   config: Lens calibration flags.
 *
 * Returns:
 *   Bitmask passed to `cv::calibrateCamera`.
 */
int calibrationFlagsFromConfig(const CalibrationFlagsConfig& config) {
    int flags = 0;
    if (config.fix_k4) {
        flags |= cv::CALIB_FIX_K4;
    }
    if (config.fix_k5) {
        flags |= cv::CALIB_FIX_K5;
    }
    if (config.fix_k6) {
        flags |= cv::CALIB_FIX_K6;
    }
    return flags;
}

bool isSupportedImage(const fs::path& path) {
    const auto extension = toLower(path.extension().string());
    return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp" ||
           extension == ".tif" || extension == ".tiff";
}

/**
 * @brief Collects supported image files from a path.
 *
 * Args:
 *   input: Image file or directory to scan recursively.
 *
 * Returns:
 *   Sorted image paths.
 */
std::vector<fs::path> collectImages(const fs::path& input) {
    if (!fs::exists(input)) {
        throw std::runtime_error("image path does not exist: " + input.string());
    }

    std::vector<fs::path> images;
    if (fs::is_regular_file(input)) {
        if (!isSupportedImage(input)) {
            throw std::runtime_error("unsupported image extension: " + input.string());
        }
        images.push_back(input);
    } else if (fs::is_directory(input)) {
        for (const auto& entry : fs::recursive_directory_iterator(input)) {
            if (entry.is_regular_file() && isSupportedImage(entry.path())) {
                images.push_back(entry.path());
            }
        }
    } else {
        throw std::runtime_error("image path is neither file nor directory: " + input.string());
    }

    std::ranges::sort(images);
    return images;
}

/**
 * @brief Computes per-frame reprojection RMSE.
 *
 * Args:
 *   objectPoints: Matched 3D board points.
 *   imagePoints: Detected 2D image points.
 *   rvec: Board-to-camera Rodrigues rotation vector.
 *   tvec: Board-to-camera translation vector.
 *   cameraMatrix: Calibrated camera matrix.
 *   distCoeffs: Calibrated distortion coefficients.
 *
 * Returns:
 *   Mean reprojection error in pixels.
 */
double meanReprojectionError(const std::vector<cv::Point3f>& objectPoints, const std::vector<cv::Point2f>& imagePoints,
                             const cv::Mat& rvec, const cv::Mat& tvec, const cv::Mat& cameraMatrix,
                             const cv::Mat& distCoeffs) {
    std::vector<cv::Point2f> projected;
    cv::projectPoints(objectPoints, rvec, tvec, cameraMatrix, distCoeffs, projected);

    double sumSquaredError = 0.0;
    for (std::size_t i = 0; i < imagePoints.size(); ++i) {
        const auto dx = static_cast<double>(imagePoints[i].x - projected[i].x);
        const auto dy = static_cast<double>(imagePoints[i].y - projected[i].y);
        sumSquaredError += (dx * dx) + (dy * dy);
    }
    return std::sqrt(sumSquaredError / static_cast<double>(imagePoints.size()));
}

/**
 * @brief Converts an OpenCV vector to `Eigen::Vector3d`.
 *
 * Args:
 *   mat: OpenCV 3-element vector.
 *
 * Returns:
 *   Eigen vector with double precision.
 */
Vector3 cvVector3(const cv::Mat& mat) {
    cv::Mat values = mat.reshape(1, 3);
    values.convertTo(values, CV_64F);
    Vector3 vector;
    cv::cv2eigen(values, vector);
    return vector;
}

/**
 * @brief Converts an OpenCV matrix to `Eigen::Matrix3d`.
 *
 * Args:
 *   mat: OpenCV 3x3 matrix.
 *
 * Returns:
 *   Eigen matrix with double precision.
 */
Matrix3 cvMatrix3(const cv::Mat& mat) {
    cv::Mat values;
    mat.convertTo(values, CV_64F);
    Matrix3 matrix;
    cv::cv2eigen(values, matrix);
    return matrix;
}

/**
 * @brief Converts OpenCV distortion coefficients to `Eigen::VectorXd`.
 *
 * Args:
 *   mat: OpenCV coefficient matrix.
 *
 * Returns:
 *   Dynamic Eigen vector with double precision.
 */
VectorX cvVectorX(const cv::Mat& mat) {
    cv::Mat values = mat.reshape(1, static_cast<int>(mat.total()));
    values.convertTo(values, CV_64F);
    VectorX vector;
    cv::cv2eigen(values, vector);
    return vector;
}

/**
 * @brief Builds a homogeneous board-to-camera transform.
 *
 * Args:
 *   rotation: Board-to-camera rotation.
 *   translation: Board-to-camera translation in meters.
 *
 * Returns:
 *   4x4 homogeneous transform.
 */
Matrix4 makeTransform(const Matrix3& rotation, const Vector3& translation) {
    Matrix4 transform = Matrix4::Identity();
    transform.block<3, 3>(0, 0) = rotation;
    transform.block<3, 1>(0, 3) = translation;
    return transform;
}

/**
 * @brief Computes the camera center in board coordinates.
 *
 * Args:
 *   rotation: Board-to-camera rotation.
 *   translation: Board-to-camera translation in meters.
 *
 * Returns:
 *   Camera center in the board frame.
 */
Vector3 cameraCenterInBoardFrame(const Matrix3& rotation, const Vector3& translation) {
    return -rotation.transpose() * translation;
}

struct DetectedView {
    fs::path imagePath;
    std::vector<int> markerIds;
    std::vector<cv::Point3f> objectPoints;
    std::vector<cv::Point2f> imagePoints;
};

} // namespace

CalibrationResult runArucoCalibration(const CalibrationConfig& config, const std::filesystem::path& sourceConfigPath) {
    validateCalibrationConfig(config);

    const auto dictionaryType = dictionaryTypeFromName(config.board.dictionary);
    if (!dictionaryType.has_value()) {
        throw std::runtime_error("unsupported board.dictionary: " + config.board.dictionary);
    }

    const auto imagePaths = collectImages(config.paths.images);
    if (imagePaths.empty()) {
        throw std::runtime_error("no supported images found under: " + config.paths.images.string());
    }

    const cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(dictionaryType.value());
    const cv::aruco::GridBoard board(cv::Size(config.board.markers_x.value(), config.board.markers_y.value()),
                                     static_cast<float>(config.board.marker_length_m.value()),
                                     static_cast<float>(config.board.marker_separation_m.value()), dictionary);

    cv::aruco::DetectorParameters detectorParameters;
    detectorParameters.cornerRefinementMethod =
        cornerRefinementMethodFromName(config.detector.corner_refinement.name());
    const cv::aruco::ArucoDetector detector(dictionary, detectorParameters);

    std::vector<DetectedView> views;
    std::optional<cv::Size> imageSize;

    for (const auto& imagePath : imagePaths) {
        const cv::Mat image = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            spdlog::warn("Could not read image: {}", imagePath.string());
            continue;
        }

        if (!imageSize.has_value()) {
            imageSize = image.size();
        } else if (image.size() != imageSize.value()) {
            spdlog::warn("Skipping image with different resolution: {}", imagePath.string());
            continue;
        }

        std::vector<std::vector<cv::Point2f>> corners;
        std::vector<std::vector<cv::Point2f>> rejectedCorners;
        std::vector<int> ids;
        detector.detectMarkers(image, corners, ids, rejectedCorners);

        if (config.detector.refine_detections && !ids.empty()) {
            detector.refineDetectedMarkers(image, board, corners, ids, rejectedCorners);
        }

        if (std::cmp_less(ids.size(), config.detector.min_markers_per_frame.value())) {
            spdlog::warn("Skipping {} with only {} detected markers", imagePath.string(), ids.size());
            continue;
        }

        DetectedView view;
        view.imagePath = imagePath;
        view.markerIds = ids;
        board.matchImagePoints(corners, ids, view.objectPoints, view.imagePoints);

        if (view.objectPoints.size() < 4 || view.imagePoints.size() < 4) {
            spdlog::warn("Skipping {} because too few board points matched", imagePath.string());
            continue;
        }

        views.push_back(std::move(view));
    }

    if (views.size() < 3) {
        throw std::runtime_error("need at least 3 accepted board views for calibration; got " +
                                 std::to_string(views.size()));
    }
    if (!imageSize.has_value()) {
        throw std::runtime_error("no readable calibration images found");
    }
    const cv::Size calibrationImageSize = imageSize.value();

    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::vector<cv::Point2f>> imagePoints;
    objectPoints.reserve(views.size());
    imagePoints.reserve(views.size());
    for (const auto& view : views) {
        objectPoints.push_back(view.objectPoints);
        imagePoints.push_back(view.imagePoints);
    }

    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    std::vector<cv::Mat> rvecs;
    std::vector<cv::Mat> tvecs;
    const double rmsError =
        cv::calibrateCamera(objectPoints, imagePoints, calibrationImageSize, cameraMatrix, distCoeffs, rvecs, tvecs,
                            calibrationFlagsFromConfig(config.calibration));

    std::vector<FrameCalibrationResult> frames;
    frames.reserve(views.size());
    for (std::size_t i = 0; i < views.size(); ++i) {
        cv::Mat rotation;
        cv::Rodrigues(rvecs[i], rotation);
        const Matrix3 rotationEigen = cvMatrix3(rotation);
        const Vector3 translationEigen = cvVector3(tvecs[i]);

        frames.push_back({
            .image = views[i].imagePath,
            .detected_marker_count = static_cast<int>(views[i].markerIds.size()),
            .matched_corner_count = static_cast<int>(views[i].imagePoints.size()),
            .mean_reprojection_error_px = meanReprojectionError(views[i].objectPoints, views[i].imagePoints, rvecs[i],
                                                                tvecs[i], cameraMatrix, distCoeffs),
            .detected_ids = views[i].markerIds,
            .rvec_board_to_camera = cvVector3(rvecs[i]),
            .tvec_board_to_camera_m = translationEigen,
            .rotation_board_to_camera = rotationEigen,
            .transform_board_to_camera = makeTransform(rotationEigen, translationEigen),
            .camera_center_board_m = cameraCenterInBoardFrame(rotationEigen, translationEigen),
        });
    }

    return CalibrationResult{
        .schema = {},
        .name = config.name,
        .source = {.config = sourceConfigPath,
                   .image_path = config.paths.images,
                   .input_image_count = static_cast<int>(imagePaths.size()),
                   .accepted_frame_count = static_cast<int>(views.size())},
        .image_size = {.width = calibrationImageSize.width, .height = calibrationImageSize.height},
        .rms_reprojection_error_px = rmsError,
        .board = config.board,
        .camera = {.matrix = cvMatrix3(cameraMatrix), .distortion_coefficients = cvVectorX(distCoeffs)},
        .frames = std::move(frames),
    };
}

int writeCoordinateSystemVisualizations(const CalibrationResult& result, const std::filesystem::path& outputDir,
                                        double axisLengthMeters) {
    if (outputDir.empty()) {
        throw std::runtime_error("visualization output directory is empty");
    }
    if (axisLengthMeters <= 0.0) {
        throw std::runtime_error("visualization axis length must be positive");
    }

    fs::create_directories(outputDir);
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    cv::eigen2cv(result.camera.matrix, cameraMatrix);
    cv::eigen2cv(result.camera.distortion_coefficients, distCoeffs);

    int written = 0;
    for (const FrameCalibrationResult& frame : result.frames) {
        cv::Mat image = cv::imread(frame.image.string(), cv::IMREAD_COLOR);
        if (image.empty()) {
            spdlog::warn("Could not read image for visualization: {}", frame.image.string());
            continue;
        }

        cv::Mat rvec;
        cv::Mat tvec;
        cv::eigen2cv(frame.rvec_board_to_camera, rvec);
        cv::eigen2cv(frame.tvec_board_to_camera_m, tvec);
        cv::drawFrameAxes(image, cameraMatrix, distCoeffs, rvec, tvec, static_cast<float>(axisLengthMeters));

        const fs::path outputPath = outputDir / frame.image.filename();
        if (!cv::imwrite(outputPath.string(), image)) {
            spdlog::warn("Could not write visualization image: {}", outputPath.string());
            continue;
        }
        ++written;
    }

    return written;
}

} // namespace covoca::calibration
