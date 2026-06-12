#include <filesystem>
#include <optional>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "covoca/calibration/CalibrationResult.h"
#include "covoca/voxel_carving/CalibratedView.h"
#include "covoca/voxel_carving/ColorImage.h"
#include "covoca/voxel_carving/SilhouetteMask.h"
#include "covoca/voxel_carving/VoxelCarver.h"
#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelColorizer.h"
#include "covoca/voxel_carving/VoxelExport.h"

namespace fs = std::filesystem;

namespace {

/**
 * Runs voxel carving from a config file.
 *
 * Loads calibration poses, silhouette masks, and optional color images, then
 * carves the configured volume and writes the requested point-cloud and mesh
 * exports.
 *
 * Args:
 *   configPath: Path to a `gs.voxel_carving.config.v1` YAML or JSON config.
 *
 * Returns:
 *   Process exit code. `0` means the run completed successfully.
 */
int run(const fs::path& configPath) {
    const auto config = covoca::voxel_carving::loadVoxelCarvingConfig(configPath);
    const auto calibration = covoca::calibration::loadCalibrationResult(config.paths.calibration_result);

    std::vector<covoca::voxel_carving::CalibratedView> views;
    views.reserve(calibration.frames.size());
    for (const auto& frame : calibration.frames) {
        const fs::path maskPath = config.paths.masks_dir / frame.image.stem().concat(".png");
        auto mask = covoca::voxel_carving::SilhouetteMask::load(maskPath, config.carving.foreground_threshold);

        std::optional<covoca::voxel_carving::ColorImage> color;
        if (config.color) {
            color = covoca::voxel_carving::ColorImage::load(frame.image);
        }

        views.emplace_back(calibration.camera, frame, std::move(mask), std::move(color));
    }

    const covoca::voxel_carving::VoxelCarver carver(config);
    const auto result = carver.run(views);

    const auto& exportConfig = config.export_config.get();
    const fs::path pointsPath = config.paths.output_dir / exportConfig.occupied_points_file;
    const fs::path meshPath = config.paths.output_dir / exportConfig.occupied_mesh_file;
    covoca::voxel_carving::writeOccupiedPoints(pointsPath, result.volume, exportConfig.format);
    covoca::voxel_carving::writeOccupiedCubeMesh(meshPath, result.volume, exportConfig.format);

    spdlog::info("Input voxels: {}, occupied: {}, carved: {}, runtime: {} s", result.stats.inputVoxelCount,
                 result.stats.occupiedVoxelCount, result.stats.carvedVoxelCount, result.stats.elapsedSeconds);
    spdlog::info("Wrote {} and {}", pointsPath.string(), meshPath.string());

    if (config.color) {
        const covoca::voxel_carving::VoxelColorizer colorizer(*config.color);
        const auto colors = colorizer.run(result.volume, views);

        const fs::path colorPointsPath = config.paths.output_dir / config.color->occupied_points_file;
        const fs::path colorMeshPath = config.paths.output_dir / config.color->occupied_mesh_file;
        covoca::voxel_carving::writeOccupiedPoints(colorPointsPath, result.volume, exportConfig.format, &colors);
        covoca::voxel_carving::writeOccupiedCubeMesh(colorMeshPath, result.volume, exportConfig.format, &colors);

        spdlog::info("Wrote {} and {} (color method: {})", colorPointsPath.string(), colorMeshPath.string(),
                     config.color->method.name());
    }

    return 0;
}

/**
 * Parses and dispatches the `voxel_carve` CLI.
 *
 * Args:
 *   argc: Argument count from `main`.
 *   argv: Argument vector from `main`.
 *
 * Returns:
 *   Process exit code from CLI parsing or the selected subcommand.
 */
int runCli(int argc, char** argv) {
    CLI::App app("Color voxel carving from calibrated multi-view silhouettes.");
    app.require_subcommand(1);
    app.footer("Example:\n"
               "  voxel_carve run --config datasets/aruco_sample/voxel_carving.yaml");

    fs::path configPath;
    CLI::App* runCommand = app.add_subcommand("run", "Carve a voxel volume and export PLY results.");
    runCommand->add_option("--config", configPath, "Voxel carving config path")->required()->check(CLI::ExistingFile);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }

    if (*runCommand) {
        return run(configPath);
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
