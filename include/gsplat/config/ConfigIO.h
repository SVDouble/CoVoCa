#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <rfl/NoExtraFields.hpp>
#include <rfl/json.hpp>
#include <rfl/yaml.hpp>

namespace gs::config {

inline std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

template <typename T> T loadTypedConfig(const std::filesystem::path& path, const std::string& description) {
    const auto extension = lowerExtension(path);
    if (extension == ".yaml" || extension == ".yml") {
        auto result = rfl::yaml::load<T, rfl::NoExtraFields>(path.string());
        if (!result) {
            throw std::runtime_error("invalid " + description + " " + path.string() + ": " + result.error().what());
        }
        return result.value();
    }

    if (extension == ".json") {
        auto result = rfl::json::load<T, rfl::NoExtraFields>(path.string());
        if (!result) {
            throw std::runtime_error("invalid " + description + " " + path.string() + ": " + result.error().what());
        }
        return result.value();
    }

    throw std::runtime_error("unsupported config extension for " + path.string() + " (expected .yaml, .yml, or .json)");
}

} // namespace gs::config
