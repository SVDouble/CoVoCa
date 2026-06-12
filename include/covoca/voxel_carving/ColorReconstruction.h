#pragma once

#include <span>

#include "covoca/voxel_carving/VoxelCarvingConfig.h"
#include "covoca/voxel_carving/VoxelColors.h"

namespace covoca::voxel_carving {

/// One view's observed color and viewing-angle weight for a voxel.
struct ColorSample {
    /// Observed color in `(red, green, blue)` order, each in `[0, 255]`.
    Rgb rgb{};

    /// `max(0, normal . view_direction)`, i.e. how head-on this view sees the
    /// voxel's estimated surface. `0` for views facing away from the surface,
    /// up to `1` for views looking straight along the normal. `1` for every
    /// sample if no normal could be estimated.
    double weight = 1.0;
};

/// Combines per-view color samples for one voxel into a single
/// `(red, green, blue)` color. Each of the four pluggable color
/// reconstruction methods is one such function, selected via
/// `ColorConfig::method`.
using ColorReconstructor = Rgb (*)(std::span<const ColorSample> samples);

/// Method 1, Color Averaging: the mean RGB value over every view in which the
/// voxel is visible and foreground.
///
/// Reference: Seitz and Dyer, "Photorealistic Scene Reconstruction by Voxel
/// Coloring", IJCV 1999.
[[nodiscard]] Rgb reconstructAverageColor(std::span<const ColorSample> samples);

/// Method 2, Best Viewpoint Selection: the color from the single view whose
/// viewing direction is most aligned with the voxel's estimated surface
/// normal, i.e. the view with the largest `ColorSample::weight`.
///
/// Reference: Debevec, Borshukov, and Yu, "Efficient View-Dependent
/// Image-Based Rendering with Projective Texture-Mapping", EGRW 1998.
[[nodiscard]] Rgb reconstructBestViewColor(std::span<const ColorSample> samples);

/// Method 3, Weighted Color Averaging: RGB averaged across views, weighted by
/// `ColorSample::weight` (viewing-direction alignment with the voxel's
/// estimated surface normal). Falls back to `reconstructAverageColor` if
/// every sample has zero weight.
///
/// Reference: Buehler, Bosse, McMillan, Gortler, and Cohen, "Unstructured
/// Lumigraph Rendering", SIGGRAPH 2001.
[[nodiscard]] Rgb reconstructWeightedAverageColor(std::span<const ColorSample> samples);

/// Method 4, Median Color Selection: the per-channel median over every
/// contributing view, reducing the influence of outlier observations (e.g.
/// occlusion errors or specular highlights) compared to averaging.
[[nodiscard]] Rgb reconstructMedianColor(std::span<const ColorSample> samples);

/**
 * Selects the color reconstruction function for a `ColorConfig::method`.
 *
 * Args:
 *   method: One of `"average"`, `"best_view"`, `"weighted_average"`, or
 *     `"median"`.
 *
 * Returns:
 *   The reconstruction function implementing the selected method.
 */
[[nodiscard]] ColorReconstructor colorReconstructorFor(ColorMethod method);

} // namespace covoca::voxel_carving
