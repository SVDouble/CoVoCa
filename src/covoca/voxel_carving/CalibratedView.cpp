#include "covoca/voxel_carving/CalibratedView.h"

#include <utility>

namespace covoca::voxel_carving {

CalibratedView::CalibratedView(calibration::CameraModel camera, calibration::FrameCalibrationResult frame,
                               SilhouetteMask mask)
    : camera_(std::move(camera)), frame_(std::move(frame)), mask_(std::move(mask)) {}

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

bool CalibratedView::isForeground(const Eigen::Vector2d& pixel) const {
    return mask_.containsForeground(pixel);
}

const std::filesystem::path& CalibratedView::imagePath() const {
    return frame_.image;
}

} // namespace covoca::voxel_carving
