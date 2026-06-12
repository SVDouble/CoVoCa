#include <filesystem>
#include <string>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "covoca/calibration/ArucoCalibration.h"
#include "covoca/calibration/CalibrationConfig.h"
#include "covoca/calibration/CalibrationResult.h"

namespace fs = std::filesystem;

namespace {

/**
 * Runs ArUco GridBoard camera calibration from a config file.
 *
 * Loads input images, estimates the shared camera intrinsics plus one
 * board-to-camera pose per accepted frame, and writes the typed calibration
 * result to the configured output path.
 *
 * Args:
 *   configPath: Path to a `gs.calibration.config.v1` YAML or JSON config.
 *
 * Returns:
 *   Process exit code. `0` means calibration completed successfully.
 */
int calibrate(const fs::path& configPath) {
    const auto config = covoca::calibration::loadCalibrationConfig(configPath);
    const auto result = covoca::calibration::runArucoCalibration(config, configPath);
    covoca::calibration::saveCalibrationResult(config.paths.result, result);

    spdlog::info("Wrote calibration result to {}", config.paths.result.string());
    spdlog::info("Accepted {} / {} images, RMS reprojection error: {} px", result.source.accepted_frame_count.value(),
                 result.source.input_image_count.value(), result.rms_reprojection_error_px.value());
    return 0;
}

/**
 * Renders coordinate-axis overlays for accepted calibration frames.
 *
 * Args:
 *   resultPath: Path to a `gs.calibration.result.v1` YAML or JSON result.
 *   outputDir: Directory where overlay images are written.
 *   axisLengthMeters: Length of each rendered coordinate axis, in meters.
 *
 * Returns:
 *   Process exit code. `0` means every overlay was written successfully.
 */
int visualize(const fs::path& resultPath, const fs::path& outputDir, double axisLengthMeters) {
    const auto result = covoca::calibration::loadCalibrationResult(resultPath);
    const int written = covoca::calibration::writeCoordinateSystemVisualizations(result, outputDir, axisLengthMeters);
    spdlog::info("Wrote {} coordinate-system visualizations to {}", written, outputDir.string());
    return 0;
}

/**
 * Parses and dispatches the `aruco_calibrate` CLI.
 *
 * Args:
 *   argc: Argument count from `main`.
 *   argv: Argument vector from `main`.
 *
 * Returns:
 *   Process exit code from CLI parsing or the selected subcommand.
 */
int runCli(int argc, char** argv) {
    CLI::App app("Estimate and inspect ArUco GridBoard camera calibration.");
    app.require_subcommand(1);
    app.footer("Examples:\n"
               "  aruco_calibrate calibrate --config datasets/aruco_sample/calibration.yaml\n"
               "  aruco_calibrate visualize --result local/datasets/aruco_sample/calibration_result.yaml "
               "--output-dir local/datasets/aruco_sample/calibration_axes --axis-length-m 0.05625");

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

/**
 * Application entrypoint.
 *
 * Args:
 *   argc: Argument count.
 *   argv: Argument vector.
 *
 * Returns:
 *   Process exit code.
 */
int main(int argc, char** argv) {
    spdlog::set_pattern("%v");

    try {
        return runCli(argc, argv);
    } catch (const std::exception& exception) {
        spdlog::error("error: {}", exception.what());
        return 1;
    }
}
