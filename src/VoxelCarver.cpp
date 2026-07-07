#include "VoxelCarver.h"

// Constructor
VoxelCarver::VoxelCarver(VoxelGrid _voxel_grid, std::vector<ObjectView> _views)
    : m_voxel_grid(std::move(_voxel_grid)), m_views(std::move(_views)) {}

// Getters
const VoxelGrid &VoxelCarver::getVoxelGrid() const { return m_voxel_grid; }

const std::vector<ObjectView> &VoxelCarver::getViews() const { return m_views; }

void VoxelCarver::carve() {
  // get all the voxels in the grid
  const auto &voxels = m_voxel_grid.getVoxelGrid();
  size_t num_voxels = voxels.size();

  // iterate through each voxel
  for (size_t i = 0; i < num_voxels; ++i) {
    // get the current voxel
    const Voxel &voxel = voxels[i];

    // check if the voxel is carved
    if (!voxel.getOccupied()) {
      continue; // skip if it is already carved
    }

    Eigen::Vector3d pos_3d =
        voxel.getCartesianPos(); // get position of the voxel in 3D space

    bool is_part_of_object =
        true; // flag to check if the voxel is part of the object

    // iterate through each view
    for (const auto &view : m_views) {
      // project the 3D position of the voxel to 2D coordinates
      auto projected_point = view.camera.projectPoint(pos_3d);

      // get int value
      int x = static_cast<int>(std::round(projected_point.x()));
      int y = static_cast<int>(std::round(projected_point.y()));

      // check if the projected point lies outside the image
      if (x < 0 || x >= view.silhouette.cols || y < 0 ||
          y >= view.silhouette.rows) {
        is_part_of_object = false; // voxel is not valid in this view
        break;
      }

      // check if the pixel in the silhouette is black (0)
      if (view.silhouette.at<uchar>(y, x) == 0) {
        is_part_of_object = false; // so it is not part of object
        break;
      }
    }

    if (!is_part_of_object) {
      m_voxel_grid.setVoxelOccupied(
          false, i); // carve the voxel by setting it to unoccupied
    }
  }
}
