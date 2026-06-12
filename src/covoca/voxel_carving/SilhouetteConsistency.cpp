#include "covoca/voxel_carving/SilhouetteConsistency.h"

#include <algorithm>
#include <array>
#include <span>

namespace covoca::voxel_carving {
namespace {

/// Fixed-capacity sample-point buffer: at most 8 corners plus the center, so
/// a plain `std::array` avoids heap allocation in the per-voxel hot loop.
struct SamplePoints {
    std::array<Eigen::Vector3d, 9> points;
    std::size_t count = 0;
};

/**
 * Builds the world-space sample points for a voxel.
 *
 * Args:
 *   volume: Voxel volume the voxel belongs to.
 *   voxel: Grid coordinates of the voxel.
 *   policy: `"center"`, `"corners"`, or `"center_and_corners"`.
 *
 * Returns:
 *   The sample points to project into each view.
 */
SamplePoints samplePoints(const VoxelVolume& volume, GridIndex voxel, const SamplePolicy& policy) {
    SamplePoints result;

    if (policy.value() != SamplePolicy::value_of<"center">()) {
        const std::array<Eigen::Vector3d, 8> corners = volume.corners(voxel);
        std::ranges::copy(corners, result.points.begin());
        result.count = corners.size();
    }
    if (policy.value() != SamplePolicy::value_of<"corners">()) {
        result.points[result.count++] = volume.center(voxel);
    }

    return result;
}

} // namespace

/**
 * Stores silhouette consistency settings.
 *
 * Args:
 *   config: Carving policy configuration.
 */
SilhouetteConsistency::SilhouetteConsistency(CarvingConfig config) : config_(config) {}

/**
 * Tests whether a voxel is consistent with every usable silhouette.
 *
 * Args:
 *   volume: Voxel volume containing `voxel`.
 *   voxel: Grid coordinates of the voxel being tested.
 *   views: Calibrated silhouette views.
 *
 * Returns:
 *   True if no non-ignored view provides background evidence for the voxel.
 */
bool SilhouetteConsistency::isConsistent(const VoxelVolume& volume, GridIndex voxel,
                                         std::span<const CalibratedView> views) const {
    const SamplePoints samples = samplePoints(volume, voxel, config_.sample_policy);
    const std::span<const Eigen::Vector3d> sampleView{samples.points.data(), samples.count};

    for (const CalibratedView& view : views) {
        bool viewIgnored = false;
        bool viewHasBackground = false;

        for (const Eigen::Vector3d& sample : sampleView) {
            const ProjectionResult projection = view.projectWorldPoint(sample);
            if (!projection.inFront || !projection.insideImage) {
                // Sample falls outside this view; outside_image_policy decides whether
                // that counts as background evidence ("carve"), is ignored for this
                // sample only ("keep"), or disqualifies the whole view ("ignore_view").
                if (config_.outside_image_policy.value() == OutsideImagePolicy::value_of<"carve">()) {
                    viewHasBackground = true;
                } else if (config_.outside_image_policy.value() == OutsideImagePolicy::value_of<"ignore_view">()) {
                    viewIgnored = true;
                    break;
                }
                continue;
            }

            if (!view.isForeground(projection.pixel)) {
                viewHasBackground = true;
            }
        }

        if (viewIgnored) {
            continue;
        }
        // A single view whose silhouette marks the voxel as background is enough
        // to carve it away; the voxel must be foreground in every view.
        if (viewHasBackground) {
            return false;
        }
    }

    return true;
}

} // namespace covoca::voxel_carving
