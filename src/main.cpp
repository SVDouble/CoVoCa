#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "DatasetLoader.h"
#include "VoxelCarver.h"
#include "VoxelCarvingConfig.h"

namespace fs = std::filesystem;

namespace {
VoxelCarvingConfig configForObject(const VoxelCarvingObjectConfig &object) {
  return VoxelCarvingConfig{object.voxel_grid, object.color};
}

fs::path outputDirFor(const VoxelCarvingBatchConfig &batch,
                      const VoxelCarvingObjectConfig &object) {
  if (object.output_dir) {
    return *object.output_dir;
  }
  if (batch.output_dir) {
    return *batch.output_dir / object.name;
  }
  return fs::path(object.name);
}

void runVoxelCarving(const fs::path &dataset_config_path,
                     const VoxelCarvingConfig &config,
                     const fs::path &output_dir) {
  LoadedDataset dataset = loadDataset(dataset_config_path);
  VoxelGrid voxel_grid = createVoxelGrid(config.voxel_grid);

  std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
  std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize()
            << std::endl;
  std::cout << "Starting voxel carving..." << std::endl;

  VoxelCarver voxel_carver(std::move(voxel_grid),
                           std::move(dataset.silhouettes),
                           std::move(dataset.cameras),
                           std::move(dataset.color_images));

  voxel_carver.carve();
  saveVoxelCarvingResult(voxel_carver.getVoxelGrid(),
                         voxel_carver.getViewVector(), config, output_dir);
}

void runVoxelCarvingBatch(const VoxelCarvingBatchConfig &config) {
  const std::size_t worker_count = std::min(
      config.objects.size(),
      std::max<std::size_t>(1, std::thread::hardware_concurrency()));
  std::atomic_size_t next_object = 0;
  std::mutex output_mutex;
  std::vector<std::string> errors;

  auto worker = [&] {
    while (true) {
      const std::size_t index = next_object.fetch_add(1);
      if (index >= config.objects.size()) {
        return;
      }

      const VoxelCarvingObjectConfig &object = config.objects[index];
      try {
        {
          std::lock_guard lock(output_mutex);
          std::cout << "[" << index + 1 << "/" << config.objects.size() << "] "
                    << object.name << std::endl;
        }
        runVoxelCarving(object.dataset_config, configForObject(object),
                        outputDirFor(config, object));
      } catch (const std::exception &exception) {
        std::lock_guard lock(output_mutex);
        errors.push_back(object.name + ": " + exception.what());
      } catch (...) {
        std::lock_guard lock(output_mutex);
        errors.push_back(object.name + ": unknown error");
      }
    }
  };

  {
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
      workers.emplace_back(worker);
    }
  }

  if (!errors.empty()) {
    std::ostringstream message;
    message << "batch voxel carving failed";
    for (const std::string &error : errors) {
      message << "\n  - " << error;
    }
    throw std::runtime_error(message.str());
  }
}

void printUsage(const char *program) {
  std::cerr
      << "Usage:\n"
      << "  " << program
      << " <dataset_config.yaml> <voxel_carving_config.yaml>\n"
      << "  " << program << " <voxel_carving_batch.yaml>\n\n"
      << "Dataset config: images_dir, masks_dir, camera_dir.\n"
      << "Voxel carving config: voxel_grid and optional color method(s).\n"
      << "Batch config: objects with dataset_config, voxel_grid, and output_dir.\n";
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    printUsage(argv[0]);
    return 1;
  }

  try {
    if (argc == 2) {
      runVoxelCarvingBatch(loadVoxelCarvingBatchConfig(argv[1]));
    } else {
      runVoxelCarving(argv[1], loadVoxelCarvingConfig(argv[2]),
                      fs::current_path());
    }

    std::cout << "End" << std::endl;
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Error: " << exception.what() << std::endl;
    return 1;
  }
}
