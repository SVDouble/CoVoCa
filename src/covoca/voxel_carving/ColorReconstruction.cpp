#include "covoca/voxel_carving/ColorReconstruction.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <Eigen/Dense>

namespace covoca::voxel_carving {
namespace {

std::uint8_t clampChannel(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

Eigen::Vector3d toVector3d(const Rgb& rgb) {
    return {static_cast<double>(rgb[0]), static_cast<double>(rgb[1]), static_cast<double>(rgb[2])};
}

Rgb toRgb(const Eigen::Vector3d& rgb) {
    return {clampChannel(rgb.x()), clampChannel(rgb.y()), clampChannel(rgb.z())};
}

} // namespace

Rgb reconstructAverageColor(std::span<const ColorSample> samples) {
    Eigen::Vector3d sum = Eigen::Vector3d::Zero();
    for (const ColorSample& sample : samples) {
        sum += toVector3d(sample.rgb);
    }
    return toRgb(sum / static_cast<double>(samples.size()));
}

Rgb reconstructBestViewColor(std::span<const ColorSample> samples) {
    const auto best = std::ranges::max_element(
        samples, [](const ColorSample& lhs, const ColorSample& rhs) { return lhs.weight < rhs.weight; });
    return best->rgb;
}

Rgb reconstructWeightedAverageColor(std::span<const ColorSample> samples) {
    double weightSum = 0.0;
    Eigen::Vector3d sum = Eigen::Vector3d::Zero();
    for (const ColorSample& sample : samples) {
        sum += sample.weight * toVector3d(sample.rgb);
        weightSum += sample.weight;
    }

    if (weightSum <= 0.0) {
        // Every view faces away from the estimated normal (or no normal could
        // be estimated); fall back to an unweighted average.
        return reconstructAverageColor(samples);
    }

    return toRgb(sum / weightSum);
}

Rgb reconstructMedianColor(std::span<const ColorSample> samples) {
    Rgb result{};
    std::vector<int> channelValues(samples.size());

    for (std::size_t channel = 0; channel < 3; ++channel) {
        for (std::size_t i = 0; i < samples.size(); ++i) {
            channelValues[i] = samples[i].rgb[channel];
        }
        std::ranges::sort(channelValues);

        const std::size_t mid = channelValues.size() / 2;
        const double median = (channelValues.size() % 2 == 0)
                                  ? (channelValues[mid - 1] + channelValues[mid]) / 2.0
                                  : static_cast<double>(channelValues[mid]);
        result[channel] = clampChannel(median);
    }

    return result;
}

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
