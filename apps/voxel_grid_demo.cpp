#include <spdlog/spdlog.h>

#include "gsplat/voxel/VoxelGrid.h"

int main() {
    const gs::voxel::VoxelGrid grid(10, 0.1);
    const gs::voxel::Voxel& voxel = grid.at(0, 0, 0);
    const auto& position = voxel.position();
    const auto& index = voxel.index();

    spdlog::info("Voxel grid: size={}, step_size={}", grid.size(), grid.stepSize());
    spdlog::info("Voxel[0,0,0]: position=[{}, {}, {}], index=[{}, {}, {}]", position.x(), position.y(), position.z(),
                 index.x(), index.y(), index.z());
}
