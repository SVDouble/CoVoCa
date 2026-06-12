#include "covoca/voxel_carving/VoxelColorizer.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "covoca/voxel_carving/ColorReconstruction.h"

namespace covoca::voxel_carving {
namespace {

/**
 * Estimates an outward-pointing surface normal for a voxel.
 *
 * Uses central differences of occupancy over the voxel's 6-neighborhood;
 * voxels outside the grid are treated as unoccupied. Occupancy increases
 * toward the interior of the volume, so the outward normal is the negated,
 * normalized occupancy gradient.
 *
 * Args:
 *   volume: Carved volume.
 *   voxel: Grid coordinates of the voxel.
 *
 * Returns:
 *   A unit outward normal, or the zero vector if the 6-neighborhood has no
 *   occupancy gradient (e.g. a fully interior voxel), signaling "no
 *   preferred viewing direction".
 */
Eigen::Vector3d estimateOutwardNormal(const VoxelVolume& volume, GridIndex voxel) {
    const GridDimensions& dims = volume.dims();
    auto occupied = [&](std::size_t x, std::size_t y, std::size_t z) -> double {
        return volume.isOccupied(GridIndex{.x = x, .y = y, .z = z}) ? 1.0 : 0.0;
    };

    const double lowerX = voxel.x == 0 ? 0.0 : occupied(voxel.x - 1, voxel.y, voxel.z);
    const double upperX = voxel.x + 1 >= dims.x ? 0.0 : occupied(voxel.x + 1, voxel.y, voxel.z);
    const double lowerY = voxel.y == 0 ? 0.0 : occupied(voxel.x, voxel.y - 1, voxel.z);
    const double upperY = voxel.y + 1 >= dims.y ? 0.0 : occupied(voxel.x, voxel.y + 1, voxel.z);
    const double lowerZ = voxel.z == 0 ? 0.0 : occupied(voxel.x, voxel.y, voxel.z - 1);
    const double upperZ = voxel.z + 1 >= dims.z ? 0.0 : occupied(voxel.x, voxel.y, voxel.z + 1);

    const Eigen::Vector3d gradient{
        upperX - lowerX,
        upperY - lowerY,
        upperZ - lowerZ,
    };

    if (gradient.isZero()) {
        return Eigen::Vector3d::Zero();
    }
    return -gradient.normalized();
}

} // namespace

/**
 * Stores color reconstruction settings.
 *
 * Args:
 *   config: Color reconstruction configuration.
 */
VoxelColorizer::VoxelColorizer(ColorConfig config) : config_(std::move(config)) {}

/**
 * Reconstructs colors for occupied voxels from calibrated views.
 *
 * Args:
 *   volume: Carved volume whose occupied voxels should be colored.
 *   views: Calibrated views with source images available for sampling.
 *
 * Returns:
 *   Per-voxel RGB colors aligned with `volume`'s flat indexing.
 */
VoxelColors VoxelColorizer::run(const VoxelVolume& volume, std::span<const CalibratedView> views) const {
    const ColorReconstructor reconstruct = colorReconstructorFor(config_.method);

    VoxelColors colors = VoxelColors::create(volume.voxelCount());
    std::vector<ColorSample> samples;
    samples.reserve(views.size());

    for (std::size_t flat = 0; flat < volume.voxelCount(); ++flat) {
        const GridIndex index = volume.gridIndex(flat);
        if (!volume.isOccupied(index)) {
            continue;
        }

        const Eigen::Vector3d center = volume.center(index);
        const Eigen::Vector3d normal = estimateOutwardNormal(volume, index);

        samples.clear();
        for (const CalibratedView& view : views) {
            const ProjectionResult projection = view.projectWorldPoint(center);
            if (!projection.inFront || !projection.insideImage || !view.isForeground(projection.pixel)) {
                continue;
            }

            const std::optional<Rgb> rgb = view.sampleColor(projection.pixel);
            if (!rgb) {
                continue;
            }

            double weight = 1.0;
            if (!normal.isZero()) {
                const Eigen::Vector3d viewDirection = (view.cameraOrigin() - center).normalized();
                weight = std::max(0.0, normal.dot(viewDirection));
            }
            samples.push_back(ColorSample{.rgb = *rgb, .weight = weight});
        }

        if (samples.empty()) {
            continue;
        }
        colors.set(flat, reconstruct(samples));
    }

    return colors;
}

} // namespace covoca::voxel_carving
