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
#include "ArucoPoseEstimator.h"
#include "ThresholdExtractor.h"
#include "PlanarHomographyExtractor.h"

int main()
{
    bool use_aruco = false;

    //
    std::vector<cv::Mat> silhouette_vector;
    std::vector<Camera> camera_vector;

    // Setup for thresholdExtractor as silhouette extractor
    ThresholdConfig thresholdConfig;
    thresholdConfig.diffThreshold = 30;
    ThresholdExtractor thresholdExtractor = ThresholdExtractor(thresholdConfig);

    /*
    // Setup for thresholdExtractor as silhouette extractor
    cv::Mat refImg = cv::imread("dataset/referenceImages/aruco_marker_board.png");
    if (refImg.empty()) {
      std::cout << "Reference image could not be read" << std::endl;
      return -1;
    }
    int dictId = cv::aruco::DICT_6X6_250;
    auto dict = cv::aruco::getPredefinedDictionary(dictId);

    std::vector<int> refIds;
    std::vector<std::vector<cv::Point2f>> refCorners;
    cv::aruco::detectMarkers(refImg, dict, refCorners, refIds);

    if (refIds.empty()) {
      std::cout << "No markers found in reference image" << std::endl;
      return -1;
    }

    PlanarHomographyConfig planarHomographyConfig;
    planarHomographyConfig.referenceImage = refImg;
    planarHomographyConfig.referenceIds = refIds;
    planarHomographyConfig.referenceCorners = refCorners;
    planarHomographyConfig.arucoDictionaryId = dictId;
    planarHomographyConfig.diffThreshold = 30;
    planarHomographyConfig.useShadowDetection = false;
    planarHomographyConfig.shadowBrightnessRatio = 0.7f;
    planarHomographyConfig.minMarkersRequired = 4;

    PlanarHomographyExtractor planarHomographyExtractor =
        PlanarHomographyExtractor(planarHomographyConfig);
    */

    // Choose the wanted silhouette extractor

    ThresholdExtractor silhouette_extractor = thresholdExtractor;
    // PlanarHomographyExtractor silhouette_extractor = planarHomographyExtractor;

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

    // Test dino
    if (!use_aruco)
    {
        std::cout << "Use Dino dataset" << std::endl;

        std::string folder = "/Users/fengvv/CoVoCa/data/Dino_Dataset/dino_selection";
        std::vector<cv::Mat> imageVector = loadImages(folder);

        for (size_t i = 0; i < imageVector.size(); ++i)
        {
            cv::Mat silhouette = silhouette_extractor.extract(imageVector[i]);
            silhouette_vector.push_back(silhouette);
            std::string filename = "silhouette_" + std::to_string(i) + ".png";
            silhouette_extractor.saveSilhouette(silhouette, filename);
        }

        std::string camera_file = "/Users/fengvv/CoVoCa/data/Dino_Dataset/dino_selection/dino_par.txt";
        camera_vector = loadCameras(camera_file);
    }
    else
    {
        std::cout << "--- AruCo---" << std::endl;

        // eigne Path
        std::string cat_folder = "";
        std::vector<cv::Mat> imageVector = loadImages(cat_folder);

        // Initialize ArucoPoseEstimator with the same board parameters
        ArucoPoseEstimator pose_estimator(5, 7, 0.04f, 0.01f, cv::aruco::DICT_6X6_250);

        cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << 1000, 0, 960, 0, 1000, 540, 0, 0, 1);
        cv::Mat dist_coeffs = cv::Mat::zeros(5, 1, CV_64F);
        pose_estimator.setIntrinsics(camera_matrix, dist_coeffs);

        // 3. Save silhouette and camera pose for each image
        for (size_t i = 0; i < imageVector.size(); ++i)
        {
            Camera cam;
            if (pose_estimator.estimateCameraPose(imageVector[i], cam))
            {
                camera_vector.push_back(cam);

                cv::Mat silhouette = silhouette_extractor.extract(imageVector[i]);
                silhouette_vector.push_back(silhouette);
            }
        }
        std::cout << camera_vector.size() << std::endl;
    }

    // Voxel Carving

    std::cout << "Starting voxel carving..." << std::endl;
    VoxelCarver voxel_carver(voxel_grid, silhouette_vector, camera_vector);

    voxel_carver.carve();
    VoxelGrid carved_voxel_grid = voxel_carver.getVoxelGrid();

    carved_voxel_grid.saveVoxelGrid();
    // voxel_carver.exportToPLY("reconstruction_result.ply");

    std::cout << "End" << std::endl;
    return 0;
}
/*
''' Voxel test_voxel = voxel_grid.getVoxel(0, 0, 0);

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
*/