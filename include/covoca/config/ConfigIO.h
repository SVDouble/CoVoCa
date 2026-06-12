#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <rfl/NoExtraFields.hpp>
#include <rfl/json.hpp>
#include <rfl/yaml.hpp>

namespace covoca::config {

/**
 * Returns a path's lowercase file extension.
 *
 * Args:
 *   path: Path whose extension should be normalized.
 *
 * Returns:
 *   Lowercase extension including the leading dot, or an empty string if the
 *   path has no extension.
 */
inline std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

/**
 * Loads a strongly typed config/result model from YAML or JSON.
 *
 * Uses reflect-cpp with `rfl::NoExtraFields`, so unknown keys are rejected
 * before domain-specific validation runs.
 *
 * Args:
 *   path: Path to a `.yaml`, `.yml`, or `.json` file.
 *   description: Human-readable model name used in error messages.
 *
 * Returns:
 *   Parsed model of type `T`.
 *
 * Throws:
 *   std::runtime_error: If the extension is unsupported, parsing fails, or
 *   reflect-cpp validation fails.
 */
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

} // namespace covoca::config
