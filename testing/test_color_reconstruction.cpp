#include "ColorReconstructor.h"
#include "VoxelGrid.h"
#include "VoxelCarver.h"
#include "View.h"
#include "Camera.h"
#include "HelperFunctions.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace fs = std::filesystem;

void testColorReconstruction(VoxelGrid& voxelGrid, const std::vector<View>& views) {

  ColorReconstructor reconstructor(voxelGrid, views);

  std::vector<std::pair<ColorMethod, std::string>> methods = {
      {ColorMethod::COLOR_AVERAGING,   "average"},
      {ColorMethod::BEST_VIEW,         "bestview"},
      {ColorMethod::WEIGHTED_AVERAGING,"weighted"},
      {ColorMethod::MEDIAN,            "median"}
  };

  // for each method, reconstruct and save
  for (const auto& [method, name] : methods) {
    std::cout << "\n=== Running " << name << " color reconstruction ===\n";
    reconstructor.reconstruct(method);

    voxelGrid.saveVoxelGrid();
    std::string finalName = "voxel_grid_" + name + ".ply";
    fs::rename("voxel_grid.ply", finalName);
    std::cout << "Saved " << finalName << "\n";
  }
}

int main(int argc, char** argv) {

  std::cout << "Current working directory: " << std::filesystem::current_path() << "\n";

  std::vector<cv::Mat> silhouette_vector;
  std::vector<Camera> camera_vector;

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

  std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
  std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;

  std::string folder_images = "../dataset/dino/images";
  std::vector<cv::Mat> imageVector = loadImages(folder_images);

  std::string folder_masks = "../dataset/dino/masks";
  std::vector<cv::Mat> silhouetteVector = loadImages(folder_masks);

  std::string camera_file = "../dataset/dino/images/dino_par.txt";
  camera_vector = loadCameras(camera_file);

  std::vector<View> views;

  if (imageVector.size() != silhouetteVector.size() ||
      imageVector.size() != camera_vector.size()) {
    std::cerr << "Error: Image, silhouette, and camera counts do not match!\n";
    return -1;
  }

  for (size_t i = 0; i < imageVector.size(); ++i) {
    View view;
    view.colorImage = imageVector[i];
    view.silhouette = silhouetteVector[i];
    view.camera = camera_vector[i];
    views.push_back(view);
  }

  std::cout << "Created " << views.size() << " views.\n";

  std::cout << "Starting voxel carving..." << std::endl;
  VoxelCarver voxel_carver(voxel_grid, silhouette_vector, camera_vector);

  voxel_carver.carve();
  VoxelGrid carved_voxel_grid = voxel_carver.getVoxelGrid();

  std::cout << "End" << std::endl;

  // test color reconstruction
  testColorReconstruction(carved_voxel_grid, views);

  // optionally also save the final hull mesh
  carved_voxel_grid.saveHullMesh();

  return 0;
}
