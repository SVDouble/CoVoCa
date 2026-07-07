#include "ColorReconstructor.h"

#include <algorithm>
#include <cmath>
#include <iostream>

ColorReconstructor::ColorReconstructor(VoxelGrid &_voxel_grid,
                                       const std::vector<ObjectView> &_views)
    : m_voxel_grid(_voxel_grid), m_views(_views) {}

// Four methods
void ColorReconstructor::reconstruct(ColorMethod method) {
  switch (method) {
  case ColorMethod::Average:
    colorAveraging();
    break;
  case ColorMethod::BestView:
    bestViewSelection();
    break;
  case ColorMethod::WeightedAverage:
    weightedColorAveraging();
    break;
  case ColorMethod::Median:
    medianColorSelection();
    break;
  }
}

// Helper: collect color samples from all views for a given voxel

std::vector<ColorReconstructor::ColorSample>
ColorReconstructor::collectColorSamples(const Voxel &voxel) const {
  std::vector<ColorSample> samples;

  Eigen::Vector3d pos_3d = voxel.getCartesianPos();

  for (const auto &view : m_views) {
    // Skip if no color image is available
    if (view.color_image.empty())
      continue;

    // Project voxel into this view
    Eigen::Vector3d pos_cam = view.camera.worldToCamera(pos_3d);
    // If the voxel is behind the camera, skip it
    if (pos_cam.z() <= 0)
      continue;

    Eigen::Vector2d projected = view.camera.projectPoint(pos_3d);
    int x = static_cast<int>(std::round(projected.x()));
    int y = static_cast<int>(std::round(projected.y()));

    // Check bounds against the color image
    if (x < 0 || x >= view.color_image.cols || y < 0 ||
        y >= view.color_image.rows)
      continue;

    // Check if the voxel projects onto the silhouette (foreground)
    if (!view.silhouette.empty() && view.silhouette.at<uchar>(y, x) == 0)
      continue;

    // Sample the color (OpenCV stores BGR)
    cv::Vec3b bgr = view.color_image.at<cv::Vec3b>(y, x);

    ColorSample sample;
    sample.color = Eigen::Vector3d(bgr[2], bgr[1], bgr[0]); // Convert to RGB

    // Distance from voxel to camera center
    Eigen::Vector3d cam_center = view.camera.getCameraCenter();
    sample.distance = (pos_3d - cam_center).norm();

    // How close the projection is to the image center (higher = better)
    double cx = view.color_image.cols / 2.0;
    double cy = view.color_image.rows / 2.0;
    double maxDist = std::sqrt(cx * cx + cy * cy);
    double distToCenter = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
    sample.centerScore =
        1.0 - (distToCenter / maxDist); // 1.0 = at center, 0.0 = at corner

    samples.push_back(sample);
  }

  return samples;
}

//  Color Averaging
void ColorReconstructor::colorAveraging() {
  std::cout << "Color Averaging" << std::endl;

  auto &voxels = m_voxel_grid.getVoxelGrid();

  for (size_t i = 0; i < voxels.size(); ++i) {
    if (!voxels[i].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[i]);

    if (samples.empty())
      continue;

    // Average all sampled colors
    Eigen::Vector3d avg_color(0.0, 0.0, 0.0);
    for (const auto &s : samples) {
      avg_color += s.color;
    }
    avg_color /= static_cast<double>(samples.size());

    m_voxel_grid.setVoxelColor(
        Eigen::Vector3i(
            std::clamp(static_cast<int>(std::round(avg_color.x())), 0, 255),
            std::clamp(static_cast<int>(std::round(avg_color.y())), 0, 255),
            std::clamp(static_cast<int>(std::round(avg_color.z())), 0, 255)),
        i);
  }
}

// Best View Selection
void ColorReconstructor::bestViewSelection() {
  std::cout << "Best View Selection" << std::endl;

  auto &voxels = m_voxel_grid.getVoxelGrid();

  for (size_t i = 0; i < voxels.size(); ++i) {
    if (!voxels[i].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[i]);

    if (samples.empty())
      continue;

    // Select the sample with the highest center score (closest to image center)
    const ColorSample *best = &samples[0];
    for (size_t j = 1; j < samples.size(); ++j) {
      if (samples[j].centerScore > best->centerScore) {
        best = &samples[j];
      }
    }

    m_voxel_grid.setVoxelColor(
        Eigen::Vector3i(
            std::clamp(static_cast<int>(std::round(best->color.x())), 0, 255),
            std::clamp(static_cast<int>(std::round(best->color.y())), 0, 255),
            std::clamp(static_cast<int>(std::round(best->color.z())), 0, 255)),
        i);
  }
}

// Weighted Averaging
void ColorReconstructor::weightedColorAveraging() {
  std::cout << "Weighted Averaging" << std::endl;

  auto &voxels = m_voxel_grid.getVoxelGrid();

  for (size_t i = 0; i < voxels.size(); ++i) {
    if (!voxels[i].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[i]);

    if (samples.empty())
      continue;

    Eigen::Vector3d weighted_color(0.0, 0.0, 0.0);
    double total_weight = 0.0;

    for (const auto &s : samples) {
      // Weight is proportional to centerScore and inversely proportional to
      // squared distance
      double weight = s.centerScore / (s.distance * s.distance + 1e-6);
      weighted_color += s.color * weight;
      total_weight += weight;
    }

    if (total_weight > 0.0) {
      weighted_color /= total_weight;
    }

    m_voxel_grid.setVoxelColor(
        Eigen::Vector3i(
            std::clamp(static_cast<int>(std::round(weighted_color.x())), 0,
                       255),
            std::clamp(static_cast<int>(std::round(weighted_color.y())), 0,
                       255),
            std::clamp(static_cast<int>(std::round(weighted_color.z())), 0,
                       255)),
        i);
  }
}

// Median Color Selection
void ColorReconstructor::medianColorSelection() {
  std::cout << "Median Color Selection" << std::endl;

  auto &voxels = m_voxel_grid.getVoxelGrid();

  for (size_t i = 0; i < voxels.size(); ++i) {
    if (!voxels[i].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[i]);

    if (samples.empty())
      continue;

    std::vector<double> r_vals;
    std::vector<double> g_vals;
    std::vector<double> b_vals;

    for (const auto &s : samples) {
      r_vals.push_back(s.color.x());
      g_vals.push_back(s.color.y());
      b_vals.push_back(s.color.z());
    }

    std::sort(r_vals.begin(), r_vals.end());
    std::sort(g_vals.begin(), g_vals.end());
    std::sort(b_vals.begin(), b_vals.end());

    size_t mid = samples.size() / 2;
    Eigen::Vector3d median_color(r_vals[mid], g_vals[mid], b_vals[mid]);

    m_voxel_grid.setVoxelColor(
        Eigen::Vector3i(
            std::clamp(static_cast<int>(std::round(median_color.x())), 0, 255),
            std::clamp(static_cast<int>(std::round(median_color.y())), 0, 255),
            std::clamp(static_cast<int>(std::round(median_color.z())), 0, 255)),
        i);
  }
}
