#pragma once

#include <filesystem>
#include <vector>

#include "ObjectView.h"

std::vector<ObjectView>
loadObjectViews(const std::filesystem::path &config_path);
