#pragma once

#include <filesystem>

#include <Eigen/Dense>
#include <opencv2/core.hpp>

namespace covoca::voxel_carving {

/// Binary foreground/background mask for one calibrated view.
class SilhouetteMask {
  public:
    /**
     * @brief Loads a grayscale mask image and binarizes it.
     *
     * Args:
     *   path: Path to a grayscale mask image.
     *   foregroundThreshold: Pixel intensity in [0, 255]; pixels at or above
     *     this value become foreground.
     *
     * Returns:
     *   Mask with all pixels thresholded to foreground (255) or background
     *   (0).
     */
    static SilhouetteMask load(const std::filesystem::path& path, int foregroundThreshold);

    /**
     * @brief Checks whether a pixel is foreground.
     *
     * Args:
     *   x: Pixel column.
     *   y: Pixel row.
     *
     * Returns:
     *   True if the pixel is inside the mask and foreground. Out-of-bounds
     *   pixels are treated as background.
     */
    [[nodiscard]] bool containsForeground(int x, int y) const;

    /**
     * @brief Checks whether a (possibly fractional) pixel is foreground.
     *
     * Args:
     *   pixel: Pixel coordinate; floored to the containing pixel.
     *
     * Returns:
     *   True if the containing pixel is inside the mask and foreground.
     */
    [[nodiscard]] bool containsForeground(const Eigen::Vector2d& pixel) const;

    /// Mask width in pixels.
    [[nodiscard]] int width() const;

    /// Mask height in pixels.
    [[nodiscard]] int height() const;

  private:
    cv::Mat mask_;
};

} // namespace covoca::voxel_carving
