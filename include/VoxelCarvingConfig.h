#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ObjectView.h"
#include "VoxelGrid.h"

struct VoxelGridConfig {
  std::array<double, 3> min;
  std::array<double, 3> max;
  std::array<int, 3> resolution;
};

struct ColorReconstructionConfig {
  std::vector<std::string> methods;
};

struct VoxelCarvingConfig {
  std::filesystem::path output_dir;
  VoxelGridConfig voxel_grid;
  std::optional<ColorReconstructionConfig> color;
};

struct VoxelCarvingObjectPaths {
  std::filesystem::path images_dir;
  std::filesystem::path masks_dir;
  std::filesystem::path camera_dir;
};

struct VoxelCarvingObjectConfig {
  std::string name;
  VoxelCarvingObjectPaths paths;
  std::optional<int> foreground_threshold;
  VoxelGridConfig voxel_grid;
  std::optional<ColorReconstructionConfig> color;
};

struct VoxelCarvingBatchConfig {
  int workers;
  std::filesystem::path output_dir;
  std::vector<VoxelCarvingObjectConfig> objects;
};

VoxelCarvingBatchConfig
loadVoxelCarvingBatchConfig(const std::filesystem::path &path);
VoxelGrid createVoxelGrid(const VoxelGridConfig &config);
void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<ObjectView> &views,
                            const VoxelCarvingConfig &config);
