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
    // Bounding box from Dino dataset README file

    double xmin = -0.041897;
    double xmax = 0.030897;

    double ymin = 0.001126;
    double ymax = 0.088227;

    double zmin = -0.037845;
    double zmax = 0.035495;

    // Initialize voxelgrid

    Eigen::Vector3d bounding_box_min(xmin, ymin, zmin);
    Eigen::Vector3d bounding_box_max(xmax, ymax, zmax);

    Eigen::Vector3i size(100, 100, 100);

    VoxelGrid voxel_grid(bounding_box_min, bounding_box_max, size);

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

    // Test camera loading
    std::string camera_file = "/home/conrad/LRZ Sync+Share/CoVoCa Datasets/Dino Dataset/dino_selection/dino_par.txt";
    std::vector<Camera> camera_vector = loadCameras(camera_file);

    // Test voxelcarving
    VoxelCarver voxel_carver(voxel_grid, silhouette_vector, camera_vector);

    voxel_carver.carve();

    VoxelGrid carved_voxel_grid = voxel_carver.getVoxelGrid();
    carved_voxel_grid.saveVoxelGrid();
    return 0;
}