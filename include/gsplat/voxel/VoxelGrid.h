#pragma once

#include <vector>

#include "gsplat/voxel/Voxel.h"

namespace gs::voxel {

class VoxelGrid {
  public:
    VoxelGrid(int size, double stepSize);

    [[nodiscard]] int size() const {
        return size_;
    }
    [[nodiscard]] double stepSize() const {
        return stepSize_;
    }
    [[nodiscard]] const std::vector<Voxel>& voxels() const {
        return voxels_;
    }
    [[nodiscard]] const Voxel& at(int x, int y, int z) const;

  private:
    int size_ = 0;
    double stepSize_ = 0.0;
    std::vector<Voxel> voxels_;
};

} // namespace gs::voxel
