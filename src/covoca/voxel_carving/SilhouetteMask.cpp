#include "covoca/voxel_carving/SilhouetteMask.h"

#include <cstdint>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace covoca::voxel_carving {

/**
 * Loads and thresholds a silhouette mask.
 *
 * Args:
 *   path: Grayscale mask image path.
 *   foregroundThreshold: Minimum pixel intensity treated as foreground.
 *
 * Returns:
 *   Binary silhouette mask.
 *
 * Throws:
 *   std::runtime_error: If OpenCV cannot decode the mask image.
 */
SilhouetteMask SilhouetteMask::load(const std::filesystem::path& path, int foregroundThreshold) {
    cv::Mat gray = cv::imread(path.string(), cv::IMREAD_GRAYSCALE);
    if (gray.empty()) {
        throw std::runtime_error("failed to load mask image " + path.string());
    }

    SilhouetteMask mask;
    cv::threshold(gray, mask.mask_, foregroundThreshold, 255, cv::THRESH_BINARY);
    return mask;
}

/**
 * Tests an integer pixel against the foreground mask.
 *
 * Args:
 *   x: Pixel column.
 *   y: Pixel row.
 *
 * Returns:
 *   True if the pixel is in bounds and foreground.
 */
bool SilhouetteMask::containsForeground(int x, int y) const {
    if (x < 0 || y < 0 || x >= mask_.cols || y >= mask_.rows) {
        return false;
    }
    return mask_.at<std::uint8_t>(y, x) != 0;
}

/**
 * Tests a fractional pixel against the foreground mask.
 *
 * Args:
 *   pixel: Pixel coordinate, floored before lookup.
 *
 * Returns:
 *   True if the containing pixel is in bounds and foreground.
 */
bool SilhouetteMask::containsForeground(const Eigen::Vector2d& pixel) const {
    return containsForeground(cvFloor(pixel.x()), cvFloor(pixel.y()));
}

/**
 * Returns the mask width.
 *
 * Returns:
 *   Width in pixels.
 */
int SilhouetteMask::width() const {
    return mask_.cols;
}

/**
 * Returns the mask height.
 *
 * Returns:
 *   Height in pixels.
 */
int SilhouetteMask::height() const {
    return mask_.rows;
}

} // namespace covoca::voxel_carving
