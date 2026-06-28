#include "ColorReconstructor.h"

#include <algorithm>
#include <iostream>
#include <cmath>

ColorReconstructor::ColorReconstructor(VoxelGrid &_voxel_grid, const std::vector<View> &_views)
    : m_voxel_grid(_voxel_grid), m_views(_views)
{
}

// Four methods 
void ColorReconstructor::reconstruct(ColorMethod method)
{
    switch (method)
    {
    case ColorMethod::COLOR_AVERAGING:
        colorAveraging();
        break;
    case ColorMethod::BEST_VIEW:
        bestViewSelection();
        break;
    case ColorMethod::WEIGHTED_AVERAGING:
        weightedColorAveraging();
        break;
    case ColorMethod::MEDIAN:
        medianColorSelection();
        break;
    }
}


// Helper: collect color samples from all views for a given voxel

std::vector<ColorReconstructor::ColorSample>
ColorReconstructor::collectColorSamples(const Voxel &voxel) const
{
    std::vector<ColorSample> samples;

    Eigen::Vector3d pos_3d = voxel.getCartesianPos();

    for (const auto &view : m_views)
    {
        // Skip if no color image is available
        if (view.colorImage.empty())
            continue;

        // Project voxel into this view
        Eigen::Vector2d projected = view.camera.projectPoint(pos_3d);
        int x = static_cast<int>(std::round(projected.x()));
        int y = static_cast<int>(std::round(projected.y()));

        // Check bounds against the color image
        if (x < 0 || x >= view.colorImage.cols ||
            y < 0 || y >= view.colorImage.rows)
            continue;

        // Check if the voxel projects onto the silhouette (foreground)
        if (!view.silhouette.empty() &&
            view.silhouette.at<uchar>(y, x) == 0)
            continue;

        // Sample the color (OpenCV stores BGR)
        cv::Vec3b bgr = view.colorImage.at<cv::Vec3b>(y, x);

        ColorSample sample;
        sample.color = Eigen::Vector3d(bgr[2], bgr[1], bgr[0]); // Convert to RGB

        // Distance from voxel to camera center
        Eigen::Vector3d cam_center = view.camera.getCameraCenter();
        sample.distance = (pos_3d - cam_center).norm();

        // How close the projection is to the image center (higher = better)
        double cx = view.colorImage.cols / 2.0;
        double cy = view.colorImage.rows / 2.0;
        double maxDist = std::sqrt(cx * cx + cy * cy);
        double distToCenter = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
        sample.centerScore = 1.0 - (distToCenter / maxDist); // 1.0 = at center, 0.0 = at corner

        samples.push_back(sample);
    }

    return samples;
}

//  Color Averaging
void ColorReconstructor::colorAveraging()
{
    std::cout << "Color Averaging" << std::endl;

    auto &voxels = m_voxel_grid.getVoxelGrid();
    int colored_count = 0;

    for (size_t i = 0; i < voxels.size(); ++i)
    {
        if (!voxels[i].getOccupied())
            continue;

        auto samples = collectColorSamples(voxels[i]);

        if (samples.empty())
            continue;

        // Average all sampled colors
        Eigen::Vector3d avg_color(0.0, 0.0, 0.0);
        for (const auto &s : samples)
        {
            avg_color += s.color;
        }
        avg_color /= static_cast<double>(samples.size());

        m_voxel_grid.setVoxelColor(
            Eigen::Vector3i(
                std::clamp(static_cast<int>(std::round(avg_color.x())), 0, 255),
                std::clamp(static_cast<int>(std::round(avg_color.y())), 0, 255),
                std::clamp(static_cast<int>(std::round(avg_color.z())), 0, 255)),
            i);

        colored_count++;
    }


}

// Best View Selection
void ColorReconstructor::bestViewSelection()
{
    std::cout << "Best View Selection" << std::endl;

    auto &voxels = m_voxel_grid.getVoxelGrid();
    int colored_count = 0;

    for (size_t i = 0; i < voxels.size(); ++i)
    {
        if (!voxels[i].getOccupied())
            continue;

        auto samples = collectColorSamples(voxels[i]);

        if (samples.empty())
            continue;

        // Select the sample with the highest center score (closest to image center)
        const ColorSample *best = &samples[0];
        for (size_t j = 1; j < samples.size(); ++j)
        {
            if (samples[j].centerScore > best->centerScore)
            {
                best = &samples[j];
            }
        }

        m_voxel_grid.setVoxelColor(
            Eigen::Vector3i(
                std::clamp(static_cast<int>(std::round(best->color.x())), 0, 255),
                std::clamp(static_cast<int>(std::round(best->color.y())), 0, 255),
                std::clamp(static_cast<int>(std::round(best->color.z())), 0, 255)),
            i);

        colored_count++;
    }

   
}

