#include "covoca/voxel_carving/ColorImage.h"

#include <stdexcept>
#include <utility>

#include <opencv2/imgcodecs.hpp>

namespace covoca::voxel_carving {

/**
 * Loads a source color image with OpenCV.
 *
 * Args:
 *   path: Image file path.
 *
 * Returns:
 *   Loaded color image.
 *
 * Throws:
 *   std::runtime_error: If OpenCV cannot decode the image.
 */
ColorImage ColorImage::load(const std::filesystem::path& path) {
    cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        throw std::runtime_error("failed to load color image " + path.string());
    }

    ColorImage result;
    result.image_ = std::move(image);
    return result;
}

/**
 * Reads one RGB pixel from the image.
 *
 * Args:
 *   pixel: Fractional pixel coordinate; floored before lookup.
 *
 * Returns:
 *   RGB color, or `std::nullopt` when `pixel` is outside the image.
 */
std::optional<Rgb> ColorImage::colorAt(const Eigen::Vector2d& pixel) const {
    const int x = cvFloor(pixel.x());
    const int y = cvFloor(pixel.y());
    if (x < 0 || y < 0 || x >= image_.cols || y >= image_.rows) {
        return std::nullopt;
    }

    const cv::Vec3b bgr = image_.at<cv::Vec3b>(y, x);
    return Rgb{bgr[2], bgr[1], bgr[0]};
}

/**
 * Returns the image width.
 *
 * Returns:
 *   Width in pixels.
 */
int ColorImage::width() const {
    return image_.cols;
}

/**
 * Returns the image height.
 *
 * Returns:
 *   Height in pixels.
 */
int ColorImage::height() const {
    return image_.rows;
}

} // namespace covoca::voxel_carving
