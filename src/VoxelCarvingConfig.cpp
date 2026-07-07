#include "VoxelCarvingConfig.h"

#include <stdexcept>
#include <utility>

#include <rfl/yaml.hpp>

#include "ColorReconstructor.h"

namespace fs = std::filesystem;

namespace {
fs::path configBase(const fs::path &path) {
  return path.has_parent_path() ? path.parent_path() : fs::current_path();
}

fs::path resolve(const fs::path &base, const fs::path &path) {
  return path.empty() || path.is_absolute() ? path : base / path;
}

ColorMethod colorMethod(const std::string &name) {
  if (name == "average")
    return ColorMethod::COLOR_AVERAGING;
  if (name == "best_view")
    return ColorMethod::BEST_VIEW;
  if (name == "weighted_average")
    return ColorMethod::WEIGHTED_AVERAGING;
  if (name == "median")
    return ColorMethod::MEDIAN;
  throw std::runtime_error("unsupported color method: " + name);
}

void saveGridFiles(VoxelGrid &voxel_grid, const fs::path &output_dir) {
  fs::create_directories(output_dir);
  voxel_grid.saveVoxelGrid(output_dir / "voxel_grid.ply");
  voxel_grid.saveHullMesh(output_dir / "voxel_hull.ply");
}

void reconstructAndSave(VoxelGrid voxel_grid, const std::vector<View> &views,
                        ColorMethod method, const fs::path &output_dir) {
  ColorReconstructor color_reconstructor(voxel_grid, views);
  color_reconstructor.reconstruct(method);
  saveGridFiles(voxel_grid, output_dir);
}

} // namespace

VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path &path) {
  auto result = rfl::yaml::load<VoxelCarvingConfig>(path.string());
  if (!result) {
    throw std::runtime_error("invalid voxel carving config " + path.string() +
                             ": " + result.error().what());
  }
  return result.value();
}

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

  const fs::path base = configBase(path);
  if (config.output_dir) {
    config.output_dir = resolve(base, *config.output_dir);
  }

  for (VoxelCarvingObjectConfig &object : config.objects) {
    if (object.name.empty()) {
      throw std::runtime_error("batch voxel carving object has an empty name");
    }
    if (object.dataset_config.empty()) {
      throw std::runtime_error("batch voxel carving object " + object.name +
                               " has no dataset_config");
    }

    object.dataset_config = resolve(base, object.dataset_config);
    if (object.output_dir) {
      object.output_dir = resolve(base, *object.output_dir);
    }
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
                            const std::vector<View> &views,
                            const VoxelCarvingConfig &config) {
  saveVoxelCarvingResult(std::move(voxel_grid), views, config,
                         fs::current_path());
}

void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<View> &views,
                            const VoxelCarvingConfig &config,
                            const std::filesystem::path &output_dir) {
  if (!config.color) {
    saveGridFiles(voxel_grid, output_dir);
    return;
  }

  const std::vector<std::string> &methods = config.color->methods;
  if (methods.empty()) {
    throw std::runtime_error("color methods list must not be empty");
  }

  if (methods.size() == 1) {
    reconstructAndSave(std::move(voxel_grid), views,
                       colorMethod(methods.front()), output_dir);
    return;
  }

  for (const std::string &name : methods) {
    reconstructAndSave(voxel_grid, views, colorMethod(name), output_dir / name);
  }
}
