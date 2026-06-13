#include <iostream>
#include <filesystem>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

#include "Voxel.h"
#include "VoxelGrid.h"
#include "Camera.h"

namespace fs = std::filesystem;

std::vector<cv::Mat> loadImages(std::string _folder)
// Loads all images (type .png, other types can easily be added) into a cv::Mat matrix and returns a vector of these matrices
{
    int i = 0;
    cv::Mat image;
    std::vector<cv::Mat> imageVector;

    for (const auto entry : fs::directory_iterator(_folder))
    {
        i++;
        image = cv::imread(entry.path().string(), cv::IMREAD_COLOR);

        std::string extension = entry.path().extension().string();
        if (extension == ".png") // other image types could be added
        {
            std::cout << "Loaded image no. "
                      << i
                      << " from "
                      << entry.path().filename()
                      << " : "
                      << image.cols << "x"
                      << image.rows << std::endl;

            imageVector.push_back(image);
        }
    }

    return imageVector;
}

int main()
{
    // Initialize VoxelGrid
    int size = 10;
    double step_size = 0.1;

    VoxelGrid voxel_grid(size, step_size);

    // Test VoxelGrid and Voxel classes
    std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
    std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;

    Voxel test_voxel = voxel_grid.getVoxel(0, 0, 0);

    std::cout << "Voxel cartesian position: " << test_voxel.getCartesianPos() << std::endl;
    std::cout << "Voxel index position: " << test_voxel.getIndexPos() << std::endl;

    // Test image loading
    std::string folder = "/home/conrad/cpp_ws/3D_Scanning/Data/dino";

    std::vector<cv::Mat> imageVector = loadImages(folder);

    return 0;
}