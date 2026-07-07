#pragma once

#include <vector>

#include "ObjectView.h"
#include "Voxel.h"
#include "VoxelGrid.h"

class VoxelCarver {
public:
  // Constructor
  VoxelCarver(VoxelGrid _voxel_grid, std::vector<ObjectView> _views);

  // Getters
  const VoxelGrid &getVoxelGrid() const;
  const std::vector<ObjectView> &getViews() const;

  // Carving method
  void carve();

private:
  VoxelGrid m_voxel_grid;
  std::vector<ObjectView> m_views;
};
