#include <iostream>
#include <filesystem>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

#include "Voxel.h"
#include "VoxelGrid.h"
#include "Camera.h"
#include "SilhouetteExtractor.h"
#include "HelperFunctions.h"
#include "VoxelCarver.h"

int main()
{
    // Initialize VoxelGrid
    int size = 1000;
    double step_size = 0.1;

    VoxelGrid voxel_grid(size, step_size);

    // Test VoxelGrid and Voxel classes
    std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
    std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;

    Voxel test_voxel = voxel_grid.getVoxel(0, 0, 0);

    std::cout << "Voxel cartesian position: " << test_voxel.getCartesianPos() << std::endl;
    std::cout << "Voxel index position: " << test_voxel.getIndexPos() << std::endl;

    // Test image loading
    std::string folder = "/home/conrad/cpp_ws/3D_Scanning/Data/dino_selection";

    std::vector<cv::Mat> imageVector = loadImages(folder);

    SilhouetteExtractor silhouette_extractor;
    std::vector<cv::Mat> silhouette_vector;
    cv::Mat silhouette;

    for (size_t i = 0; i < imageVector.size(); ++i)
    {
        silhouette = silhouette_extractor.extract(imageVector[i]);
        silhouette_vector.push_back(silhouette);

        std::string filename = "silhouette_" + std::to_string(i) + ".png";
        silhouette_extractor.saveSilhouette(silhouette, filename);
    }

    std::string camera_file = "/home/conrad/LRZ Sync+Share/CoVoCa Datasets/Dino Dataset/dino_selection/dino_par.txt";
    std::vector<Camera> camera_vector = loadCameras(camera_file);

    VoxelCarver voxel_carver(voxel_grid, silhouette_vector, camera_vector);
    return 0;
}