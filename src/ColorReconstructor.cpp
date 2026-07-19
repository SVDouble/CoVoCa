#include "ColorReconstructor.h"

#include <algorithm>
#include <cmath>
#include <iostream>

ColorReconstructor::ColorReconstructor(VoxelGrid &_voxel_grid,
                                       const std::vector<ObjectView> &_views)
    : m_voxel_grid(_voxel_grid), m_views(_views) {}

// Four methods
void ColorReconstructor::reconstruct(ColorMethod method) {

  // Build one depth (z-)buffer per view before doing any color sampling.
  buildDepthBuffers();

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

// Build a per-view depth buffer holding the squared camera-distance of the
// closest occupied voxel seen at each pixel, plus small per-view constants
void ColorReconstructor::buildDepthBuffers() {
  const auto &voxels = m_voxel_grid.getVoxelGrid();
  const size_t n_views = m_views.size();

  m_view_depth.assign(n_views, cv::Mat());
  m_view_constants.assign(n_views, ViewConstants{});

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (long v = 0; v < static_cast<long>(n_views); ++v) {
    const auto &view = m_views[static_cast<size_t>(v)];
    if (view.color_image.empty())
      continue;

    cv::Mat depth(view.color_image.rows, view.color_image.cols, CV_64F,
                  cv::Scalar(std::numeric_limits<double>::max()));

    ViewConstants vc;
    vc.cx = view.color_image.cols / 2.0;
    vc.cy = view.color_image.rows / 2.0;
    vc.maxDistSq = vc.cx * vc.cx + vc.cy * vc.cy;
    // cache the camera center once per view
    vc.cam_center = view.camera.getCameraCenter();

    for (size_t i = 0; i < voxels.size(); ++i) {
      if (!voxels[i].getOccupied())
        continue;

      Eigen::Vector3d pos_3d = voxels[i].getCartesianPos();
      Eigen::Vector3d pixel_h = view.camera.projectPointHomogeneous(pos_3d);
      if (pixel_h.z() <= 0)
        continue;

      int x = static_cast<int>(std::round(pixel_h.x() / pixel_h.z()));
      int y = static_cast<int>(std::round(pixel_h.y() / pixel_h.z()));
      if (x < 0 || x >= depth.cols || y < 0 || y >= depth.rows)
        continue;

      if (!view.silhouette.empty() && view.silhouette.at<uchar>(y, x) == 0)
        continue;

      double distSq = (pos_3d - vc.cam_center).squaredNorm();
      double &best = depth.at<double>(y, x);
      if (distSq < best)
        best = distSq;
    }

    m_view_depth[static_cast<size_t>(v)] = std::move(depth);
    m_view_constants[static_cast<size_t>(v)] = vc;
  }
}

// Bilinear sample of a BGR cv::Mat at a continuous (sub-pixel) coordinate
Eigen::Vector3d ColorReconstructor::bilinearSampleBGR(const cv::Mat &image,
                                                      double xf, double yf) {
  int x0 = static_cast<int>(std::floor(xf));
  int y0 = static_cast<int>(std::floor(yf));
  int x1 = std::min(x0 + 1, image.cols - 1);
  int y1 = std::min(y0 + 1, image.rows - 1);
  x0 = std::clamp(x0, 0, image.cols - 1);
  y0 = std::clamp(y0, 0, image.rows - 1);

  double fx = xf - std::floor(xf);
  double fy = yf - std::floor(yf);

  const cv::Vec3b &c00 = image.at<cv::Vec3b>(y0, x0);
  const cv::Vec3b &c10 = image.at<cv::Vec3b>(y0, x1);
  const cv::Vec3b &c01 = image.at<cv::Vec3b>(y1, x0);
  const cv::Vec3b &c11 = image.at<cv::Vec3b>(y1, x1);

  Eigen::Vector3d rgb;
  for (int ch = 0; ch < 3; ++ch) {
    int bgr_idx = 2 - ch; // BGR -> RGB
    double top = c00[bgr_idx] * (1 - fx) + c10[bgr_idx] * fx;
    double bottom = c01[bgr_idx] * (1 - fx) + c11[bgr_idx] * fx;
    rgb[ch] = top * (1 - fy) + bottom * fy;
  }
  return rgb;
}

// Helper: collect color samples from all views for a given voxel
std::vector<ColorReconstructor::ColorSample>
ColorReconstructor::collectColorSamples(const Voxel &voxel) const {
  std::vector<ColorSample> samples;
  samples.reserve(m_views.size());

  Eigen::Vector3d pos_3d = voxel.getCartesianPos();

  const double voxel_size = m_voxel_grid.getStepSize().norm();
  const double tol = voxel_size * 1.5;
  const double depth_tolerance_sq = tol * tol;

  for (size_t v = 0; v < m_views.size(); ++v) {
    const auto &view = m_views[v];
    if (view.color_image.empty() || m_view_depth[v].empty())
      continue;

    Eigen::Vector3d pixel_h = view.camera.projectPointHomogeneous(pos_3d);
    if (pixel_h.z() <= 0)
      continue;

    double xf = pixel_h.x() / pixel_h.z();
    double yf = pixel_h.y() / pixel_h.z();

    // Bounds check against the +1 neighbors needed for bilinear sampling.
    if (xf < 0 || xf >= view.color_image.cols - 1 || yf < 0 ||
        yf >= view.color_image.rows - 1)
      continue;

    int xn = static_cast<int>(std::round(xf));
    int yn = static_cast<int>(std::round(yf));

    // Check if the voxel projects onto the silhouette (foreground)
    if (!view.silhouette.empty() && view.silhouette.at<uchar>(yn, xn) == 0)
      continue;

    const ViewConstants &vc = m_view_constants[v];
    double distSq = (pos_3d - vc.cam_center).squaredNorm();

    // occlusion test
    double closestSq = m_view_depth[v].at<double>(yn, xn);
    if (distSq > closestSq + depth_tolerance_sq)
      continue;

    ColorSample sample;
    sample.color = bilinearSampleBGR(view.color_image, xf, yf);
    sample.distance_sq = distSq;

    double dxr = xn - vc.cx;
    double dyr = yn - vc.cy;
    double distToCenterSq = dxr * dxr + dyr * dyr;
    sample.centerScore =
        1.0 - std::sqrt(distToCenterSq / vc.maxDistSq); // 1.0 = at center, 0.0 = at corner

    samples.push_back(sample);
  }

  return samples;
}


//  Color Averaging
void ColorReconstructor::colorAveraging() {
  std::cout << "Color Averaging" << std::endl;

  auto &voxels = m_voxel_grid.getVoxelGrid();
  const long n = static_cast<long>(voxels.size());

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (long i = 0; i < n; ++i) {
    if (!voxels[static_cast<size_t>(i)].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[static_cast<size_t>(i)]);
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
        static_cast<int>(i));
  }
}

// Best View Selection
void ColorReconstructor::bestViewSelection() {
  std::cout << "Best View Selection" << std::endl;

  const auto &voxels = m_voxel_grid.getVoxelGrid();
  const long n = static_cast<long>(voxels.size());

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (long i = 0; i < n; ++i) {
    if (!voxels[static_cast<size_t>(i)].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[static_cast<size_t>(i)]);
    if (samples.empty())
      continue;

    // Select the sample with the highest score combining centerScore and distance
    const ColorSample *best = &samples[0];
    double best_score = best->centerScore / (best->distance_sq + 1e-6);

    for (size_t j = 1; j < samples.size(); ++j) {
      double current_score = samples[j].centerScore / (samples[j].distance_sq + 1e-6);
      if (current_score > best_score) {
        best = &samples[j];
        best_score = current_score;
      }
    }

    m_voxel_grid.setVoxelColor(
        Eigen::Vector3i(
            std::clamp(static_cast<int>(std::round(best->color.x())), 0, 255),
            std::clamp(static_cast<int>(std::round(best->color.y())), 0, 255),
            std::clamp(static_cast<int>(std::round(best->color.z())), 0, 255)),
        static_cast<int>(i));
  }
}


// Weighted Averaging
void ColorReconstructor::weightedColorAveraging() {
  std::cout << "Weighted Averaging" << std::endl;

  const auto &voxels = m_voxel_grid.getVoxelGrid();
  const long n = static_cast<long>(voxels.size());

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (long i = 0; i < n; ++i) {
    if (!voxels[static_cast<size_t>(i)].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[static_cast<size_t>(i)]);
    if (samples.empty())
      continue;

    Eigen::Vector3d weighted_color(0.0, 0.0, 0.0);
    double total_weight = 0.0;

    for (const auto &s : samples) {
      // Weight is proportional to centerScore and inversely proportional to
      // squared distance
      double weight = s.centerScore / (s.distance_sq + 1e-6);
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
        static_cast<int>(i));
  }
}


// Median Color Selection
void ColorReconstructor::medianColorSelection() {
  std::cout << "Median Color Selection" << std::endl;

  const auto &voxels = m_voxel_grid.getVoxelGrid();
  const long n = static_cast<long>(voxels.size());

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
  for (long i = 0; i < n; ++i) {
    if (!voxels[static_cast<size_t>(i)].getOccupied())
      continue;

    auto samples = collectColorSamples(voxels[static_cast<size_t>(i)]);
    if (samples.empty())
      continue;

    std::vector<double> r_vals;
    std::vector<double> g_vals;
    std::vector<double> b_vals;
    r_vals.reserve(samples.size());
    g_vals.reserve(samples.size());
    b_vals.reserve(samples.size());

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
        static_cast<int>(i));
  }
}
