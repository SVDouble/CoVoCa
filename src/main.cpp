#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <rfl/yaml.hpp>

#include "ColorReconstructor.h"
#include "DatasetLoader.h"
#include "VoxelCarver.h"
#include "VoxelGrid.h"

namespace
{
struct Grid { std::array<double, 3> min; std::array<double, 3> max; std::array<int, 3> resolution; };
struct Color { std::string method; };

struct CarvingConfig { Grid voxel_grid; std::optional<Color> color; };

ColorMethod parseColorMethod(const std::string &method)
{
    if (method == "average") return ColorMethod::COLOR_AVERAGING;
    if (method == "best_view") return ColorMethod::BEST_VIEW;
    if (method == "weighted_average") return ColorMethod::WEIGHTED_AVERAGING;
    if (method == "median") return ColorMethod::MEDIAN;
    throw std::runtime_error("unknown color method: " + method);
}

CarvingConfig loadCarvingConfig(const std::filesystem::path &path)
{
    auto result = rfl::yaml::load<CarvingConfig>(path.string());
    if (!result)
    {
        throw std::runtime_error("invalid voxel carving config " + path.string() + ": " + result.error().what());
    }
    return result.value();
}

void printUsage(const char *program)
{
    std::cerr << "Usage: " << program << " <dataset_config.yaml> <voxel_carving_config.yaml>\n\n"
              << "Dataset config: images_dir, masks_dir, camera_file.\n"
              << "Voxel carving config: voxel_grid and optional color method.\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        LoadedDataset dataset = loadDataset(argv[1]);
        CarvingConfig config = loadCarvingConfig(argv[2]);
        const Grid &grid = config.voxel_grid;
        VoxelGrid voxel_grid(Eigen::Vector3d(grid.min[0], grid.min[1], grid.min[2]),
                             Eigen::Vector3d(grid.max[0], grid.max[1], grid.max[2]),
                             Eigen::Vector3i(grid.resolution[0], grid.resolution[1], grid.resolution[2]));

        std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
        std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;
        std::cout << "Starting voxel carving..." << std::endl;

        VoxelCarver voxel_carver(std::move(voxel_grid),
                                 std::move(dataset.silhouettes),
                                 std::move(dataset.cameras),
                                 std::move(dataset.color_images));

        voxel_carver.carve();
        VoxelGrid carved_voxel_grid = voxel_carver.getVoxelGrid();

        if (config.color)
        {
            ColorReconstructor color_reconstructor(carved_voxel_grid, voxel_carver.getViewVector());
            color_reconstructor.reconstruct(parseColorMethod(config.color->method));
        }

        carved_voxel_grid.saveVoxelGrid();
        carved_voxel_grid.saveHullMesh();

        std::cout << "End" << std::endl;
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Error: " << exception.what() << std::endl;
        return 1;
    }
}
