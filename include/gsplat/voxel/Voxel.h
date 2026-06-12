#pragma once

#include <Eigen/Dense>
#include <utility>

namespace gs::voxel {

class Voxel {
  public:
    Voxel() = default;
    Voxel(Eigen::Vector3d position, Eigen::Vector3i index) : position_(std::move(position)), index_(std::move(index)) {}

    [[nodiscard]] const Eigen::Vector3d& position() const {
        return position_;
    }
    [[nodiscard]] const Eigen::Vector3i& index() const {
        return index_;
    }

  private:
    Eigen::Vector3d position_{0.0, 0.0, 0.0};
    Eigen::Vector3i index_{-1, -1, -1};
};

} // namespace gs::voxel
