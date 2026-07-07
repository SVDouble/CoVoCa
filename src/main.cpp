#include <algorithm>
#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ObjectDataLoader.h"
#include "VoxelCarver.h"
#include "VoxelCarvingConfig.h"

namespace {
void carveObject(const VoxelCarvingObjectConfig &object,
                 const VoxelCarvingConfig &config) {
  std::vector<ObjectView> object_views = loadObjectViews(object);
  VoxelGrid voxel_grid = createVoxelGrid(config.voxel_grid);

  std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
  std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;
  std::cout << "Starting voxel carving..." << std::endl;

  VoxelCarver voxel_carver(std::move(voxel_grid), std::move(object_views));

  voxel_carver.carve();
  saveVoxelCarvingResult(voxel_carver.getVoxelGrid(), voxel_carver.getViews(),
                         config);
}

void carveBatch(const VoxelCarvingBatchConfig &config) {
  const std::size_t worker_count =
      std::min(config.objects.size(), static_cast<std::size_t>(config.workers));
  std::atomic_size_t next_object = 0;
  std::mutex output_mutex;
  std::vector<std::string> errors;

  auto worker = [&] {
    while (true) {
      // A shared index keeps the worker count fixed while distributing objects.
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

        carveObject(object,
                    VoxelCarvingConfig{config.output_dir / object.name,
                                       object.voxel_grid, object.color});
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

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr
        << "Usage:\n  " << argv[0] << " <voxel_carving_batch.yaml>\n\n"
        << "The batch config contains workers, output_dir, and one or more "
           "objects with paths and voxel_grid.\n";
    return 1;
  }

  try {
    carveBatch(loadVoxelCarvingBatchConfig(argv[1]));

    std::cout << "End" << std::endl;
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "Error: " << exception.what() << std::endl;
    return 1;
  }
}
