#include "DatasetLoader.h"

#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <rfl/yaml.hpp>

namespace fs = std::filesystem;

namespace
{
constexpr std::array<const char *, 8> kImageExtensions = {
    ".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".ppm", ".pgm"};

struct Paths { fs::path images_dir; fs::path masks_dir; fs::path camera_file; };

struct Config {
    Paths paths;
    std::optional<int> foreground_threshold;
};

[[noreturn]] void fail(const std::string &message) { throw std::runtime_error(message); }

fs::path resolve(const fs::path &base, const fs::path &path)
{
    return path.empty() || path.is_absolute() ? path : base / path;
}

Config loadConfig(const fs::path &path)
{
    auto result = rfl::yaml::load<Config>(path.string());
    if (!result)
    {
        fail("invalid dataset config " + path.string() + ": " + result.error().what());
    }

    Config config = result.value();
    const fs::path base = path.has_parent_path() ? path.parent_path() : fs::current_path();
    config.paths.images_dir = resolve(base, config.paths.images_dir);
    config.paths.masks_dir = resolve(base, config.paths.masks_dir);
    config.paths.camera_file = resolve(base, config.paths.camera_file);
    return config;
}

template <typename Matrix>
void readMatrix(std::istream &input, Matrix &matrix)
{
    for (int row = 0; row < matrix.rows(); ++row)
    {
        for (int col = 0; col < matrix.cols(); ++col)
        {
            if (!(input >> matrix(row, col)))
            {
                fail("invalid camera file");
            }
        }
    }
}

fs::path findByNameOrStem(const fs::path &directory, const fs::path &name)
{
    const fs::path exact = directory / name.filename();
    if (fs::is_regular_file(exact))
    {
        return exact;
    }

    const std::string stem = name.stem().string();
    for (const char *extension : kImageExtensions)
    {
        const fs::path candidate = directory / (stem + extension);
        if (fs::is_regular_file(candidate))
        {
            return candidate;
        }
    }
    fail("could not find " + name.filename().string() + " in " + directory.string());
}

cv::Mat readImage(const fs::path &path, int flags)
{
    cv::Mat image = cv::imread(path.string(), flags);
    if (image.empty())
    {
        fail("failed to load image: " + path.string());
    }
    return image;
}

} // namespace

LoadedDataset loadDataset(const fs::path &config_path)
{
    Config config = loadConfig(config_path);
    if (!fs::is_directory(config.paths.images_dir))
    {
        fail("images_dir is not a directory: " + config.paths.images_dir.string());
    }
    if (!fs::is_directory(config.paths.masks_dir))
    {
        fail("masks_dir is not a directory: " + config.paths.masks_dir.string());
    }
    if (!fs::is_regular_file(config.paths.camera_file))
    {
        fail("camera_file is not a file: " + config.paths.camera_file.string());
    }

    std::ifstream camera_file(config.paths.camera_file);
    int camera_count = 0;
    if (!camera_file || !(camera_file >> camera_count) || camera_count <= 0)
    {
        fail("invalid camera file");
    }

    LoadedDataset dataset;
    dataset.cameras.reserve(static_cast<std::size_t>(camera_count));
    dataset.silhouettes.reserve(static_cast<std::size_t>(camera_count));
    dataset.color_images.reserve(static_cast<std::size_t>(camera_count));

    for (int i = 0; i < camera_count; ++i)
    {
        std::string image_name;
        Eigen::Matrix3d intrinsics;
        Eigen::Matrix3d rotation;
        Eigen::Vector3d translation;
        if (!(camera_file >> image_name))
        {
            fail("invalid camera file");
        }
        readMatrix(camera_file, intrinsics);
        readMatrix(camera_file, rotation);
        readMatrix(camera_file, translation);

        const fs::path image_path = findByNameOrStem(config.paths.images_dir, image_name);
        cv::Mat image = readImage(image_path, cv::IMREAD_COLOR);
        const fs::path mask_path = findByNameOrStem(config.paths.masks_dir, image_path);
        cv::Mat mask = readImage(mask_path, cv::IMREAD_GRAYSCALE);
        cv::threshold(mask, mask, config.foreground_threshold.value_or(1), 255, cv::THRESH_BINARY);

        if (mask.empty() || mask.size() != image.size())
        {
            fail("invalid silhouette for " + image_path.string());
        }

        dataset.cameras.emplace_back(intrinsics, rotation, translation);
        dataset.silhouettes.push_back(std::move(mask));
        dataset.color_images.push_back(std::move(image));
    }

    std::cout << "Loaded " << dataset.cameras.size() << " calibrated views" << std::endl;
    return dataset;
}
