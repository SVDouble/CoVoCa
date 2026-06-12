#include "covoca/voxel_carving/CalibratedView.h"

#include <utility>

namespace covoca::voxel_carving {

/**
 * Stores the calibration, mask, and optional color image for one view.
 *
 * Args:
 *   camera: Shared camera intrinsics.
 *   frame: Per-frame extrinsics and image metadata.
 *   mask: Silhouette mask for the frame.
 *   color: Optional source color image for color reconstruction.
 */
CalibratedView::CalibratedView(calibration::CameraModel camera, calibration::FrameCalibrationResult frame,
                               SilhouetteMask mask, std::optional<ColorImage> color)
    : camera_(std::move(camera)), frame_(std::move(frame)), mask_(std::move(mask)), color_(std::move(color)) {}

/**
 * Projects a board/world point into this frame.
 *
 * Args:
 *   pointWorld: Point in calibration board coordinates, in meters.
 *
 * Returns:
 *   Pixel coordinate, depth, and visibility flags for this view.
 */
ProjectionResult CalibratedView::projectWorldPoint(const Eigen::Vector3d& pointWorld) const {
    // board_to_camera: X_camera = R * X_world + t.
    const Eigen::Vector3d pointCamera = frame_.rotation_board_to_camera * pointWorld + frame_.tvec_board_to_camera_m;

    ProjectionResult result;
    result.depthMeters = pointCamera.z();
    result.inFront = pointCamera.z() > 0.0;
    if (!result.inFront) {
        // Behind the camera; pixel/insideImage are left at their default (zero/false).
        return result;
    }

    const auto& matrix = camera_.matrix;
    const double fx = matrix(0, 0);
    const double fy = matrix(1, 1);
    const double cx = matrix(0, 2);
    const double cy = matrix(1, 2);

    // Pinhole projection: divide by depth, then apply intrinsics. No lens
    // distortion correction is applied.
    result.pixel =
        Eigen::Vector2d{(fx * pointCamera.x() / pointCamera.z()) + cx, (fy * pointCamera.y() / pointCamera.z()) + cy};
    result.insideImage = result.pixel.x() >= 0.0 && result.pixel.y() >= 0.0 && result.pixel.x() < mask_.width() &&
                         result.pixel.y() < mask_.height();
    return result;
}

/**
 * Checks whether a projected pixel is foreground.
 *
 * Args:
 *   pixel: Pixel coordinate in this frame.
 *
 * Returns:
 *   True if `pixel` maps to a foreground mask pixel.
 */
bool CalibratedView::isForeground(const Eigen::Vector2d& pixel) const {
    return mask_.containsForeground(pixel);
}

/**
 * Returns the source image path for this frame.
 *
 * Returns:
 *   Image path stored in the calibration result.
 */
const std::filesystem::path& CalibratedView::imagePath() const {
    return frame_.image;
}

/**
 * Samples this frame's source image at a projected pixel.
 *
 * Args:
 *   pixel: Pixel coordinate in this frame.
 *
 * Returns:
 *   RGB color if this view has a color image and the pixel is inside it.
 */
std::optional<Rgb> CalibratedView::sampleColor(const Eigen::Vector2d& pixel) const {
    if (!color_) {
        return std::nullopt;
    }
    return color_->colorAt(pixel);
}

/**
 * Computes the camera center in board/world coordinates.
 *
 * Returns:
 *   Camera optical center derived from the stored board-to-camera transform.
 */
Eigen::Vector3d CalibratedView::cameraOrigin() const {
    // board_to_camera: X_camera = R * X_world + t, so X_world = R^T * (X_camera - t).
    // The camera's optical center is X_camera = 0, giving C = -R^T * t.
    return -frame_.rotation_board_to_camera.transpose() * frame_.tvec_board_to_camera_m;
}

} // namespace covoca::voxel_carving
