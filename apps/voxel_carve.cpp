#include <filesystem>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "covoca/calibration/CalibrationResult.h"
#include "covoca/voxel_carving/CalibratedView.h"
#include "covoca/voxel_carving/SilhouetteMask.h"
#include "covoca/voxel_carving/VoxelCarver.h"
#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelExport.h"

namespace fs = std::filesystem;

namespace {

int run(const fs::path& configPath) {
    const auto config = covoca::voxel_carving::loadVoxelCarvingConfig(configPath);
    const auto calibration = covoca::calibration::loadCalibrationResult(config.paths.calibration_result);

    std::vector<covoca::voxel_carving::CalibratedView> views;
    views.reserve(calibration.frames.size());
    for (const auto& frame : calibration.frames) {
        const fs::path maskPath = config.paths.masks_dir / frame.image.stem().concat(".png");
        auto mask = covoca::voxel_carving::SilhouetteMask::load(maskPath, config.carving.foreground_threshold);
        views.emplace_back(calibration.camera, frame, std::move(mask));
    }

    const covoca::voxel_carving::VoxelCarver carver(config);
    const auto result = carver.run(views);

    const fs::path pointsPath = config.paths.output_dir / config.export_config.get().occupied_points_ply;
    const fs::path meshPath = config.paths.output_dir / config.export_config.get().occupied_mesh_ply;
    covoca::voxel_carving::writeOccupiedPointsPly(pointsPath, result.volume);
    covoca::voxel_carving::writeOccupiedCubeMeshPly(meshPath, result.volume);

    spdlog::info("Input voxels: {}, occupied: {}, carved: {}, runtime: {} s", result.stats.inputVoxelCount,
                 result.stats.occupiedVoxelCount, result.stats.carvedVoxelCount, result.stats.elapsedSeconds);
    spdlog::info("Wrote {} and {}", pointsPath.string(), meshPath.string());
    return 0;
}

int runCli(int argc, char** argv) {
    CLI::App app("Color voxel carving from calibrated multi-view silhouettes.");
    app.require_subcommand(1);
    app.footer("Example:\n"
               "  voxel_carve run --config configs/voxel_carving/sample.yaml");

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

int main(int argc, char** argv) {
    spdlog::set_pattern("%v");

    try {
        return runCli(argc, argv);
    } catch (const std::exception& exception) {
        spdlog::error("error: {}", exception.what());
        return 1;
    }
}
