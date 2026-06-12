#pragma once

#include <filesystem>
#include <optional>

#include <Eigen/Dense>
#include <opencv2/core.hpp>

#include "covoca/voxel_carving/VoxelColors.h"

namespace covoca::voxel_carving {

/// Color source image for one calibrated view, used for color reconstruction.
class ColorImage {
  public:
    /**
     * @brief Loads a color image.
     *
     * Args:
     *   path: Path to the frame's source image (the same image used for
     *     calibration).
     *
     * Returns:
     *   Loaded image, stored internally in BGR order as decoded by OpenCV.
     */
    static ColorImage load(const std::filesystem::path& path);

    /**
     * @brief Samples the RGB color at a (possibly fractional) pixel.
     *
     * Args:
     *   pixel: Pixel coordinate; floored to the containing pixel.
     *
     * Returns:
     *   The pixel's color as `(red, green, blue)` in `[0, 255]`, or
     *   `std::nullopt` if the pixel is outside the image.
     */
    [[nodiscard]] std::optional<Rgb> colorAt(const Eigen::Vector2d& pixel) const;

    /// Image width in pixels.
    [[nodiscard]] int width() const;

    /// Image height in pixels.
    [[nodiscard]] int height() const;

  private:
    cv::Mat image_;
};

} // namespace covoca::voxel_carving
