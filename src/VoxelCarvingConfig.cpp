#include "VoxelCarvingConfig.h"

#include <stdexcept>
#include <utility>

#include <rfl/yaml.hpp>

#include "ColorReconstructor.h"

namespace fs = std::filesystem;

namespace {
fs::path resolve(const fs::path &base, const fs::path &path) {
  return path.empty() || path.is_absolute() ? path : base / path;
}

ColorMethod colorMethod(const std::string &name) {
  if (name == "average")
    return ColorMethod::Average;
  if (name == "best_view")
    return ColorMethod::BestView;
  if (name == "weighted_average")
    return ColorMethod::WeightedAverage;
  if (name == "median")
    return ColorMethod::Median;
  throw std::runtime_error("unsupported color method: " + name);
}

void saveVoxelGridAndHull(VoxelGrid &voxel_grid, const fs::path &output_dir) {
  fs::create_directories(output_dir);
  voxel_grid.saveVoxelGrid(output_dir / "voxel_grid.ply");
  voxel_grid.saveHullMesh(output_dir / "voxel_hull.ply");
}

void reconstructColorAndSave(VoxelGrid voxel_grid,
                             const std::vector<ObjectView> &views,
                             ColorMethod method, const fs::path &output_dir) {
  ColorReconstructor color_reconstructor(voxel_grid, views);
  color_reconstructor.reconstruct(method);
  saveVoxelGridAndHull(voxel_grid, output_dir);
}

} // namespace

VoxelCarvingBatchConfig
loadVoxelCarvingBatchConfig(const std::filesystem::path &path) {
  auto result = rfl::yaml::load<VoxelCarvingBatchConfig>(path.string());
  if (!result) {
    throw std::runtime_error("invalid batch voxel carving config " +
                             path.string() + ": " + result.error().what());
  }

  VoxelCarvingBatchConfig config = result.value();
  if (config.objects.empty()) {
    throw std::runtime_error("batch voxel carving config has no objects: " +
                             path.string());
  }
  if (config.workers < 1) {
    throw std::runtime_error(
        "batch voxel carving config workers must be >= 1: " + path.string());
  }
  if (config.output_dir.empty()) {
    throw std::runtime_error("batch voxel carving config has no output_dir: " +
                             path.string());
  }

  const fs::path base =
      path.has_parent_path() ? path.parent_path() : fs::current_path();
  config.output_dir = resolve(base, config.output_dir);

  for (VoxelCarvingObjectConfig &object : config.objects) {
    if (object.name.empty()) {
      throw std::runtime_error("batch voxel carving object has an empty name");
    }
    if (object.paths.images_dir.empty() || object.paths.masks_dir.empty() ||
        object.paths.camera_dir.empty()) {
      throw std::runtime_error("batch voxel carving object " + object.name +
                               " has incomplete paths");
    }

    object.paths.images_dir = resolve(base, object.paths.images_dir);
    object.paths.masks_dir = resolve(base, object.paths.masks_dir);
    object.paths.camera_dir = resolve(base, object.paths.camera_dir);
  }

  return config;
}

VoxelGrid createVoxelGrid(const VoxelGridConfig &config) {
  return VoxelGrid(Eigen::Vector3d(config.min[0], config.min[1], config.min[2]),
                   Eigen::Vector3d(config.max[0], config.max[1], config.max[2]),
                   Eigen::Vector3i(config.resolution[0], config.resolution[1],
                                   config.resolution[2]));
}

void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<ObjectView> &views,
                            const VoxelCarvingConfig &config) {
  // Output paths come from config so runs do not depend on the launch
  // directory.
  if (!config.color) {
    saveVoxelGridAndHull(voxel_grid, config.output_dir);
    return;
  }

  const std::vector<std::string> &methods = config.color->methods;
  if (methods.empty()) {
    throw std::runtime_error("color methods list must not be empty");
  }

  if (methods.size() == 1) {
    reconstructColorAndSave(std::move(voxel_grid), views,
                            colorMethod(methods.front()), config.output_dir);
    return;
  }

  for (const std::string &name : methods) {
    reconstructColorAndSave(voxel_grid, views, colorMethod(name),
                            config.output_dir / name);
  }
}
