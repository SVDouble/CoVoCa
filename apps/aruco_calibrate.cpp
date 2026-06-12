#include <filesystem>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "gsplat/calibration/ArucoCalibration.h"
#include "gsplat/calibration/CalibrationConfig.h"
#include "gsplat/calibration/CalibrationResult.h"

namespace fs = std::filesystem;

namespace {

int calibrate(const fs::path& configPath) {
    const auto config = gs::calibration::loadCalibrationConfig(configPath);
    const auto result = gs::calibration::runArucoCalibration(config, configPath);
    gs::calibration::saveCalibrationResult(config.paths.result, result);

    spdlog::info("Wrote calibration result to {}", config.paths.result.string());
    spdlog::info("Accepted {} / {} images, RMS reprojection error: {} px", result.source.accepted_frame_count.value(),
                 result.source.input_image_count.value(), result.rms_reprojection_error_px.value());
    return 0;
}

int visualize(const fs::path& resultPath, const fs::path& outputDir, double axisLengthMeters) {
    const auto result = gs::calibration::loadCalibrationResult(resultPath);
    const int written = gs::calibration::writeCoordinateSystemVisualizations(result, outputDir, axisLengthMeters);
    spdlog::info("Wrote {} coordinate-system visualizations to {}", written, outputDir.string());
    return 0;
}

int runCli(int argc, char** argv) {
    CLI::App app("Estimate and inspect ArUco GridBoard camera calibration.");
    app.require_subcommand(1);
    app.footer("Examples:\n"
               "  aruco_calibrate calibrate --config configs/calibration/aruco_sample.yaml\n"
               "  aruco_calibrate visualize --result data/sample_aruco/calibration_result.yaml "
               "--output-dir data/sample_aruco/calibration_axes --axis-length-m 0.05625");

    fs::path configPath;
    CLI::App* calibrateCommand = app.add_subcommand("calibrate", "Estimate intrinsics and board-to-camera extrinsics.");
    calibrateCommand->add_option("--config", configPath, "Calibration config path")
        ->required()
        ->check(CLI::ExistingFile);

    fs::path resultPath;
    fs::path outputDir;
    double axisLengthMeters = 0.0;
    CLI::App* visualizeCommand =
        app.add_subcommand("visualize", "Draw calibrated board/world coordinate axes into accepted frames.");
    visualizeCommand->add_option("--result", resultPath, "Calibration result path")
        ->required()
        ->check(CLI::ExistingFile);
    visualizeCommand->add_option("--output-dir", outputDir, "Visualization output dir")->required();
    visualizeCommand->add_option("--axis-length-m", axisLengthMeters, "Rendered axis length in meters")
        ->required()
        ->check(CLI::PositiveNumber);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }

    if (*calibrateCommand) {
        return calibrate(configPath);
    }
    if (*visualizeCommand) {
        return visualize(resultPath, outputDir, axisLengthMeters);
    }

    return 1;
}

} // namespace

int main(int argc, char** argv) {
    spdlog::set_pattern("%v");

    try {
        return runCli(argc, argv);
    } catch (const std::exception& exception) {
        spdlog::error("error: {}", exception.what());
        return 1;
    }
}
