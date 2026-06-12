#include "covoca/voxel_carving/ColorImage.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include <opencv2/imgcodecs.hpp>

namespace covoca::voxel_carving {

ColorImage ColorImage::load(const std::filesystem::path& path) {
    cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
        throw std::runtime_error("failed to load color image " + path.string());
    }

    ColorImage result;
    result.image_ = std::move(image);
    return result;
}

std::optional<Rgb> ColorImage::colorAt(const Eigen::Vector2d& pixel) const {
    const int x = static_cast<int>(std::floor(pixel.x()));
    const int y = static_cast<int>(std::floor(pixel.y()));
    if (x < 0 || y < 0 || x >= image_.cols || y >= image_.rows) {
        return std::nullopt;
    }

    const cv::Vec3b bgr = image_.at<cv::Vec3b>(y, x);
    return Rgb{bgr[2], bgr[1], bgr[0]};
}

int ColorImage::width() const {
    return image_.cols;
}

int ColorImage::height() const {
    return image_.rows;
}

} // namespace covoca::voxel_carving
