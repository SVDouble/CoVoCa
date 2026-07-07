#include "ColorReconstructor.h"
#include "ObjectView.h"
#include "VoxelGrid.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

namespace {
const Eigen::Vector3i kExpectedRgb(10, 20, 30);

VoxelGrid makeGrid() {
  return VoxelGrid(Eigen::Vector3d(1.0, 1.0, 1.0),
                   Eigen::Vector3d(1.0, 1.0, 1.0), Eigen::Vector3i(2, 2, 2));
}

std::vector<ObjectView> makeViews() {
  cv::Mat color_image(3, 3, CV_8UC3, cv::Scalar(30, 20, 10));
  cv::Mat silhouette(3, 3, CV_8UC1, cv::Scalar(255));
  return {ObjectView{Camera(), std::move(silhouette), std::move(color_image)}};
}

bool allVoxelsHaveColor(const VoxelGrid &grid, const Eigen::Vector3i &color) {
  for (const Voxel &voxel : grid.getVoxelGrid()) {
    if (voxel.getColor() != color) {
      return false;
    }
  }
  return true;
}

bool methodPasses(ColorMethod method) {
  VoxelGrid grid = makeGrid();
  std::vector<ObjectView> views = makeViews();
  ColorReconstructor reconstructor(grid, views);
  reconstructor.reconstruct(method);
  return allVoxelsHaveColor(grid, kExpectedRgb);
}

} // namespace

int main() {
  const std::array methods = {
      ColorMethod::Average,
      ColorMethod::BestView,
      ColorMethod::WeightedAverage,
      ColorMethod::Median,
  };

  for (ColorMethod method : methods) {
    if (!methodPasses(method)) {
      std::cerr << "Color reconstruction method produced an unexpected color\n";
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
