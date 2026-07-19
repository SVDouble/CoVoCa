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
    double distance_sq;       // Squared distance from voxel to camera center
    double centerScore;    // How close the projection is to the image center
                           // (normalized)
  };

  // Per-view values that used to be recomputed for every single voxel/sample
  // that touched that view, now computed once in buildDepthBuffers()
  struct ViewConstants {
    double cx, cy, maxDistSq;
    Eigen::Vector3d cam_center;
  };

  std::vector<ColorSample> collectColorSamples(const Voxel &voxel) const;

  // Builds one depth (z-)buffer per view: for each pixel, the squared
  // camera-distance of the closest occupied voxel that projects there
  void buildDepthBuffers();

  // Bilinear sample of a BGR cv::Mat at a continuous (sub-pixel) coordinate,
  // returned as an RGB Eigen::Vector3d.
  static Eigen::Vector3d bilinearSampleBGR(const cv::Mat &image, double xf,
                                           double yf);

  VoxelGrid &m_voxel_grid;
  const std::vector<ObjectView> &m_views;
  std::vector<cv::Mat> m_view_depth;
  std::vector<ViewConstants> m_view_constants;
};
