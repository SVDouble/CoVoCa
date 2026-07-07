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

struct VoxelCarvingObjectConfig {
  std::string name;
  std::filesystem::path object_config;
  VoxelGridConfig voxel_grid;
  std::optional<ColorReconstructionConfig> color;
};

struct VoxelCarvingBatchConfig {
  int workers;
  std::filesystem::path output_dir;
  std::vector<VoxelCarvingObjectConfig> objects;
};

VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path &path);
VoxelCarvingBatchConfig
loadVoxelCarvingBatchConfig(const std::filesystem::path &path);
VoxelGrid createVoxelGrid(const VoxelGridConfig &config);
void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<ObjectView> &views,
                            const VoxelCarvingConfig &config);
