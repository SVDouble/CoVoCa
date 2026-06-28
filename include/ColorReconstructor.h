#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include "VoxelGrid.h"
#include "View.h"

// Four Methodes for color reconstruction
enum class ColorMethod
{
    COLOR_AVERAGING,        // Simple average of all visible colors
    BEST_VIEW,              // Pick the view with the best viewing angle
    WEIGHTED_AVERAGING,     // Weighted average based on distance to camera center
    MEDIAN                  // Median color selection per channel
};

class ColorReconstructor
{
public:
    ColorReconstructor(VoxelGrid &_voxel_grid, const std::vector<View> &_views);

    void reconstruct(ColorMethod method);

    void colorAveraging();
    void bestViewSelection();
    void weightedColorAveraging();
    void medianColorSelection();

private:
    // Helper: collect valid color samples for a voxel from all views
    // Returns vector of (color, camera_center) pairs for each view where the voxel is visible
    struct ColorSample
    {
        Eigen::Vector3d color; // RGB in [0, 255]
        double distance;       // Distance from voxel to camera center
        double centerScore;    // How close the projection is to the image center (normalized)
    };

    std::vector<ColorSample> collectColorSamples(const Voxel &voxel) const;

    VoxelGrid &m_voxel_grid;
    const std::vector<View> &m_views;
};
