#pragma once

#include <filesystem>

#include <Eigen/Dense>

#include "covoca/calibration/CalibrationResult.h"
#include "covoca/voxel_carving/SilhouetteMask.h"

namespace covoca::voxel_carving {

/// Result of projecting a world point into one calibrated view.
struct ProjectionResult {
    bool inFront = false;
    bool insideImage = false;
    Eigen::Vector2d pixel = Eigen::Vector2d::Zero();
    double depthMeters = 0.0;
};

/// One calibrated camera frame, paired with its silhouette mask.
class CalibratedView {
  public:
    /**
     * @brief Builds a view from a camera model, frame pose, and mask.
     *
     * Args:
     *   camera: Intrinsics shared by all frames in the calibration result.
     *   frame: Per-frame board-to-camera pose and image path.
     *   mask: Loaded silhouette mask for this frame's image.
     */
    CalibratedView(calibration::CameraModel camera, calibration::FrameCalibrationResult frame, SilhouetteMask mask);

    /**
     * @brief Projects a world-space point into this view's image.
     *
     * Transforms the point into camera space using the frame's
     * board-to-camera pose, then applies the pinhole projection (no
     * distortion correction).
     *
     * Args:
     *   pointWorld: 3D point in world (board) coordinates, in meters.
     *
     * Returns:
     *   Projection outcome, including the pixel coordinate, whether the
     *   point is in front of the camera, and whether it falls inside the
     *   image bounds.
     */
    [[nodiscard]] ProjectionResult projectWorldPoint(const Eigen::Vector3d& pointWorld) const;

    /**
     * @brief Checks the silhouette mask at a projected pixel.
     *
     * Args:
     *   pixel: Pixel coordinate, typically from `projectWorldPoint`.
     *
     * Returns:
     *   True if the pixel is foreground in this view's mask.
     */
    [[nodiscard]] bool isForeground(const Eigen::Vector2d& pixel) const;

    /// Path to this frame's source image.
    [[nodiscard]] const std::filesystem::path& imagePath() const;

  private:
    calibration::CameraModel camera_;
    calibration::FrameCalibrationResult frame_;
    SilhouetteMask mask_;
};

} // namespace covoca::voxel_carving
