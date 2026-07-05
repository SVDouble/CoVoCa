#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "ColorReconstructor.h"
#include "DatasetLoader.h"
#include "VoxelCarver.h"

namespace
{
ColorMethod parseColorMethod(const std::string &method)
{
    if (method == "average") return ColorMethod::COLOR_AVERAGING;
    if (method == "best_view") return ColorMethod::BEST_VIEW;
    if (method == "weighted_average") return ColorMethod::WEIGHTED_AVERAGING;
    if (method == "median") return ColorMethod::MEDIAN;
    throw std::runtime_error("unknown color method: " + method);
}

void printUsage(const char *program)
{
    std::cerr << "Usage: " << program << " <dataset_config.yaml>\n\n"
              << "The config schema is covoca.branch1.dataset.v1.\n"
              << "Required paths: images_dir, masks_dir, camera_file.\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        LoadedDataset dataset = loadDataset(std::filesystem::path(argv[1]));

        std::cout << "Voxelgrid size: " << dataset.voxel_grid.getSize() << std::endl;
        std::cout << "Voxelgrid stepsize: " << dataset.voxel_grid.getStepSize() << std::endl;
        std::cout << "Starting voxel carving..." << std::endl;

        VoxelCarver voxel_carver(std::move(dataset.voxel_grid),
                                 std::move(dataset.silhouettes),
                                 std::move(dataset.cameras),
                                 std::move(dataset.color_images));

        voxel_carver.carve();
        VoxelGrid carved_voxel_grid = voxel_carver.getVoxelGrid();

        if (dataset.color_method)
        {
            ColorReconstructor color_reconstructor(carved_voxel_grid, voxel_carver.getViewVector());
            color_reconstructor.reconstruct(parseColorMethod(*dataset.color_method));
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
