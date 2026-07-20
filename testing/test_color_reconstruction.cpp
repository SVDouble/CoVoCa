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

ObjectView makeView(const Eigen::Vector3d &camera_center,
                    const Eigen::Matrix3d &rotation,
                    const cv::Scalar &bgr_color) {
  Eigen::Matrix3d intrinsics = Eigen::Matrix3d::Identity();
  intrinsics(0, 2) = 1.0;
  intrinsics(1, 2) = 1.0;
  Camera camera(intrinsics, rotation, -rotation * camera_center);
  return ObjectView{camera, cv::Mat(3, 3, CV_8UC1, cv::Scalar(255)),
                    cv::Mat(3, 3, CV_8UC3, bgr_color)};
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

bool normalWeightFavorsHeadOnView() {
  VoxelGrid grid(Eigen::Vector3d(-1.0, -1.0, 1.0),
                 Eigen::Vector3d(1.0, 1.0, 3.0), Eigen::Vector3i(3, 3, 3));
  for (int i = 0; i < static_cast<int>(grid.getVoxelGrid().size()); ++i) {
    grid.setVoxelOccupied(false, i);
  }

  const auto flatIndex = [](int x, int y, int z) {
    return x + 3 * (y + 3 * z);
  };
  grid.setVoxelOccupied(true, flatIndex(1, 1, 1));
  grid.setVoxelOccupied(true, flatIndex(2, 1, 1));

  Eigen::Matrix3d front_rotation;
  front_rotation << 0.0, 1.0, 0.0, 0.0, 0.0, -1.0, -1.0, 0.0, 0.0;
  Eigen::Matrix3d side_rotation;
  side_rotation << -1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, -1.0, 0.0;

  std::vector<ObjectView> views;
  views.push_back(makeView(Eigen::Vector3d(3.0, 0.0, 2.0), front_rotation,
                           cv::Scalar(0, 0, 200)));
  views.push_back(makeView(Eigen::Vector3d(1.0, 3.0, 2.0), side_rotation,
                           cv::Scalar(200, 0, 0)));

  ColorReconstructor(grid, views)
      .reconstruct(ColorMethod::NormalWeightedAverage);
  return grid.getVoxel(2, 1, 1).getColor() == Eigen::Vector3i(200, 0, 0);
}

} // namespace

int main() {
  const std::array methods = {
      ColorMethod::Average,         ColorMethod::BestView,
      ColorMethod::WeightedAverage, ColorMethod::NormalWeightedAverage,
      ColorMethod::Median,
  };

  for (ColorMethod method : methods) {
    if (!methodPasses(method)) {
      std::cerr << "Color reconstruction method produced an unexpected color\n";
      return EXIT_FAILURE;
    }
  }

  if (!normalWeightFavorsHeadOnView()) {
    std::cerr << "Normal-weighted reconstruction ignored view alignment\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
