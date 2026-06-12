#include "covoca/voxel_carving/SilhouetteMask.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace covoca::voxel_carving {

SilhouetteMask SilhouetteMask::load(const std::filesystem::path& path, int foregroundThreshold) {
    cv::Mat gray = cv::imread(path.string(), cv::IMREAD_GRAYSCALE);
    if (gray.empty()) {
        throw std::runtime_error("failed to load mask image " + path.string());
    }

    SilhouetteMask mask;
    cv::threshold(gray, mask.mask_, foregroundThreshold, 255, cv::THRESH_BINARY);
    return mask;
}

bool SilhouetteMask::containsForeground(int x, int y) const {
    if (x < 0 || y < 0 || x >= mask_.cols || y >= mask_.rows) {
        return false;
    }
    return mask_.at<std::uint8_t>(y, x) != 0;
}

bool SilhouetteMask::containsForeground(const Eigen::Vector2d& pixel) const {
    return containsForeground(static_cast<int>(std::floor(pixel.x())), static_cast<int>(std::floor(pixel.y())));
}

int SilhouetteMask::width() const {
    return mask_.cols;
}

int SilhouetteMask::height() const {
    return mask_.rows;
}

} // namespace covoca::voxel_carving
