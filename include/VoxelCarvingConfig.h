#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "View.h"
#include "VoxelGrid.h"

struct VoxelGridConfig {
  std::array<double, 3> min;
  std::array<double, 3> max;
  std::array<int, 3> resolution;
};

struct ColorConfig {
  std::vector<std::string> methods;
};

struct VoxelCarvingConfig {
  VoxelGridConfig voxel_grid;
  std::optional<ColorConfig> color;
};

struct VoxelCarvingObjectConfig {
  std::string name;
  std::filesystem::path dataset_config;
  std::optional<std::filesystem::path> output_dir;
  VoxelGridConfig voxel_grid;
  std::optional<ColorConfig> color;
};

struct VoxelCarvingBatchConfig {
  std::optional<std::filesystem::path> output_dir;
  std::vector<VoxelCarvingObjectConfig> objects;
};

VoxelCarvingConfig loadVoxelCarvingConfig(const std::filesystem::path &path);
VoxelCarvingBatchConfig
loadVoxelCarvingBatchConfig(const std::filesystem::path &path);
VoxelGrid createVoxelGrid(const VoxelGridConfig &config);
void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<View> &views,
                            const VoxelCarvingConfig &config);
void saveVoxelCarvingResult(VoxelGrid voxel_grid,
                            const std::vector<View> &views,
                            const VoxelCarvingConfig &config,
                            const std::filesystem::path &output_dir);
