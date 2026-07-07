#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

#include "ObjectView.h"
#include "VoxelGrid.h"

enum class ColorMethod {
  Average,
  BestView,
  WeightedAverage,
  Median,
};

class ColorReconstructor {
public:
  ColorReconstructor(VoxelGrid &_voxel_grid,
                     const std::vector<ObjectView> &_views);

  void reconstruct(ColorMethod method);

  void colorAveraging();
  void bestViewSelection();
  void weightedColorAveraging();
  void medianColorSelection();

private:
  // Helper: collect valid color samples for a voxel from all views
  // Returns vector of (color, camera_center) pairs for each view where the
  // voxel is visible
  struct ColorSample {
    Eigen::Vector3d color; // RGB in [0, 255]
    double distance;       // Distance from voxel to camera center
    double centerScore;    // How close the projection is to the image center
                           // (normalized)
  };

  std::vector<ColorSample> collectColorSamples(const Voxel &voxel) const;

  VoxelGrid &m_voxel_grid;
  const std::vector<ObjectView> &m_views;
};
