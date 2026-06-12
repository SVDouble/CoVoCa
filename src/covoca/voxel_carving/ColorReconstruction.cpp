#include "covoca/voxel_carving/ColorReconstruction.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <Eigen/Dense>

namespace covoca::voxel_carving {
namespace {

/**
 * Rounds and clamps a floating-point RGB channel.
 *
 * Args:
 *   value: Channel value before conversion to 8-bit storage.
 *
 * Returns:
 *   `value` rounded to the nearest integer and clamped to `[0, 255]`.
 */
std::uint8_t clampChannel(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

/**
 * Averages RGB samples with equal weight.
 *
 * Args:
 *   samples: Non-empty color observations for one voxel.
 *
 * Returns:
 *   Mean RGB color.
 */
Rgb reconstructAverageColor(std::span<const ColorSample> samples) {
    Eigen::Vector3d sum = Eigen::Vector3d::Zero();
    for (const ColorSample& sample : samples) {
        Eigen::Vector3d rgb;
        rgb << sample.rgb[0], sample.rgb[1], sample.rgb[2];
        sum += rgb;
    }
    const auto sampleCount = static_cast<double>(samples.size());
    const Eigen::Vector3d rgb = sum / sampleCount;
    return {clampChannel(rgb.x()), clampChannel(rgb.y()), clampChannel(rgb.z())};
}

/**
 * Selects the color from the highest-weight sample.
 *
 * Args:
 *   samples: Non-empty color observations for one voxel.
 *
 * Returns:
 *   RGB color from the sample with the largest `ColorSample::weight`.
 */
Rgb reconstructBestViewColor(std::span<const ColorSample> samples) {
    const auto best = std::ranges::max_element(
        samples, [](const ColorSample& lhs, const ColorSample& rhs) { return lhs.weight < rhs.weight; });
    return best->rgb;
}

/**
 * Averages RGB samples using their viewing-angle weights.
 *
 * Args:
 *   samples: Non-empty color observations for one voxel.
 *
 * Returns:
 *   Weighted mean RGB color, or the unweighted mean if all weights are zero.
 */
Rgb reconstructWeightedAverageColor(std::span<const ColorSample> samples) {
    double weightSum = 0.0;
    Eigen::Vector3d sum = Eigen::Vector3d::Zero();
    for (const ColorSample& sample : samples) {
        Eigen::Vector3d rgb;
        rgb << sample.rgb[0], sample.rgb[1], sample.rgb[2];
        sum += sample.weight * rgb;
        weightSum += sample.weight;
    }

    if (weightSum <= 0.0) {
        // Every view faces away from the estimated normal (or no normal could
        // be estimated); fall back to an unweighted average.
        return reconstructAverageColor(samples);
    }

    const Eigen::Vector3d rgb = sum / weightSum;
    return {clampChannel(rgb.x()), clampChannel(rgb.y()), clampChannel(rgb.z())};
}

/**
 * Computes the per-channel median color.
 *
 * Args:
 *   samples: Non-empty color observations for one voxel.
 *
 * Returns:
 *   RGB color whose channels are the independent medians of the sample
 *   channels.
 */
Rgb reconstructMedianColor(std::span<const ColorSample> samples) {
    Rgb result{};
    std::vector<int> channelValues(samples.size());

    for (std::size_t channel = 0; channel < Rgb{}.size(); ++channel) {
        for (std::size_t i = 0; i < samples.size(); ++i) {
            channelValues[i] = samples[i].rgb[channel];
        }
        std::ranges::sort(channelValues);

        const std::size_t mid = channelValues.size() / 2;
        const double median =
            (channelValues.size() % 2 == 0) ? (channelValues[mid - 1] + channelValues[mid]) / 2.0 : channelValues[mid];
        result[channel] = clampChannel(median);
    }

    return result;
}

/**
 * Maps a configured color method to its implementation function.
 *
 * Args:
 *   method: Configured color reconstruction method.
 *
 * Returns:
 *   Function pointer implementing `method`.
 */
ColorReconstructor colorReconstructorFor(ColorMethod method) {
    if (method.value() == ColorMethod::value_of<"best_view">()) {
        return reconstructBestViewColor;
    }
    if (method.value() == ColorMethod::value_of<"weighted_average">()) {
        return reconstructWeightedAverageColor;
    }
    if (method.value() == ColorMethod::value_of<"median">()) {
        return reconstructMedianColor;
    }
    return reconstructAverageColor;
}

} // namespace covoca::voxel_carving
