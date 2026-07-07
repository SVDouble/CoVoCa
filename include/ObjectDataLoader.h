#pragma once

#include <vector>

#include "ObjectView.h"

struct VoxelCarvingObjectConfig;

std::vector<ObjectView> loadObjectViews(const VoxelCarvingObjectConfig &object);
