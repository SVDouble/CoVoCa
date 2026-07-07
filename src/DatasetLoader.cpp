#include "DatasetLoader.h"

#include <array>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <rfl/yaml.hpp>

namespace fs = std::filesystem;

namespace {
constexpr std::array<const char *, 8> kImageExtensions = {
    ".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".ppm", ".pgm"};

struct Paths {
  fs::path images_dir;
  fs::path masks_dir;
  fs::path camera_dir;
};

struct Config {
  Paths paths;
  std::optional<int> foreground_threshold;
};

struct IntrinsicsProfile {
  std::vector<std::vector<double>> matrix;
};
struct IntrinsicsDocument {
  std::map<std::string, IntrinsicsProfile> profiles;
};

struct PoseFrame {
  std::string image;
  std::string intrinsics_profile;
  std::vector<std::vector<double>> rotation_board_to_camera;
  std::vector<double> tvec_board_to_camera_m;
};

struct PosesDocument {
  std::vector<PoseFrame> frames;
};

struct CalibratedImage {
  std::string image_name;
  Camera camera;
};

fs::path resolve(const fs::path &base, const fs::path &path) {
  return path.empty() || path.is_absolute() ? path : base / path;
}

Config loadConfig(const fs::path &path) {
  auto result = rfl::yaml::load<Config>(path.string());
  if (!result) {
    throw std::runtime_error("invalid dataset config " + path.string() + ": " +
                             result.error().what());
  }

  Config config = result.value();
  const fs::path base =
      path.has_parent_path() ? path.parent_path() : fs::current_path();
  config.paths.images_dir = resolve(base, config.paths.images_dir);
  config.paths.masks_dir = resolve(base, config.paths.masks_dir);
  config.paths.camera_dir = resolve(base, config.paths.camera_dir);
  return config;
}

fs::path findByNameOrStem(const fs::path &directory, const fs::path &name) {
  const fs::path exact = directory / name.filename();
  if (fs::is_regular_file(exact)) {
    return exact;
  }

  const std::string stem = name.stem().string();
  for (const char *extension : kImageExtensions) {
    const fs::path candidate = directory / (stem + extension);
    if (fs::is_regular_file(candidate)) {
      return candidate;
    }
  }
  throw std::runtime_error("could not find " + name.filename().string() +
                           " in " + directory.string());
}

cv::Mat readImage(const fs::path &path, int flags) {
  cv::Mat image = cv::imread(path.string(), flags);
  if (image.empty()) {
    throw std::runtime_error("failed to load image: " + path.string());
  }
  return image;
}

Eigen::Matrix3d toMatrix3d(const std::vector<std::vector<double>> &values,
                           const std::string &label) {
  if (values.size() != 3) {
    throw std::runtime_error(label + " must have exactly 3 rows");
  }

  Eigen::Matrix3d matrix;
  for (std::size_t row = 0; row < values.size(); ++row) {
    if (values[row].size() != 3) {
      throw std::runtime_error(label + " must have exactly 3 columns");
    }
    for (std::size_t col = 0; col < values[row].size(); ++col) {
      matrix(static_cast<int>(row), static_cast<int>(col)) = values[row][col];
    }
  }
  return matrix;
}

Eigen::Vector3d toVector3d(const std::vector<double> &values,
                           const std::string &label) {
  if (values.size() != 3) {
    throw std::runtime_error(label + " must have exactly 3 values");
  }
  return Eigen::Vector3d(values[0], values[1], values[2]);
}

template <typename T>
T loadYamlFile(const fs::path &path, const std::string &label) {
  auto result = rfl::yaml::load<T>(path.string());
  if (!result) {
    throw std::runtime_error("invalid " + label + " " + path.string() + ": " +
                             result.error().what());
  }
  return result.value();
}

std::vector<CalibratedImage> loadCameraRecords(const fs::path &camera_dir) {
  if (!fs::is_directory(camera_dir)) {
    throw std::runtime_error("camera_dir is not a directory: " +
                             camera_dir.string());
  }
  const fs::path intrinsics_path = camera_dir / "intrinsics.yaml";
  const fs::path poses_path = camera_dir / "poses.yaml";
  if (!fs::is_regular_file(intrinsics_path)) {
    throw std::runtime_error("camera intrinsics file is not a file: " +
                             intrinsics_path.string());
  }
  if (!fs::is_regular_file(poses_path)) {
    throw std::runtime_error("camera poses file is not a file: " +
                             poses_path.string());
  }

  const IntrinsicsDocument intrinsics = loadYamlFile<IntrinsicsDocument>(
      intrinsics_path, "camera intrinsics file");
  const PosesDocument poses =
      loadYamlFile<PosesDocument>(poses_path, "camera poses file");
  if (poses.frames.empty()) {
    throw std::runtime_error("camera poses file has no frames: " +
                             poses_path.string());
  }

  std::vector<CalibratedImage> records;
  records.reserve(poses.frames.size());
  for (const PoseFrame &frame : poses.frames) {
    const auto profile = intrinsics.profiles.find(frame.intrinsics_profile);
    if (profile == intrinsics.profiles.end()) {
      throw std::runtime_error("unknown intrinsics profile " +
                               frame.intrinsics_profile + " for " +
                               frame.image);
    }

    records.push_back({
        frame.image,
        Camera(
            toMatrix3d(profile->second.matrix, "intrinsics matrix"),
            toMatrix3d(frame.rotation_board_to_camera,
                       "rotation_board_to_camera"),
            toVector3d(frame.tvec_board_to_camera_m, "tvec_board_to_camera_m")),
    });
  }
  return records;
}

} // namespace

LoadedDataset loadDataset(const fs::path &config_path) {
  Config config = loadConfig(config_path);
  if (!fs::is_directory(config.paths.images_dir)) {
    throw std::runtime_error("images_dir is not a directory: " +
                             config.paths.images_dir.string());
  }
  if (!fs::is_directory(config.paths.masks_dir)) {
    throw std::runtime_error("masks_dir is not a directory: " +
                             config.paths.masks_dir.string());
  }
  std::vector<CalibratedImage> camera_records =
      loadCameraRecords(config.paths.camera_dir);

  LoadedDataset dataset;
  dataset.cameras.reserve(camera_records.size());
  dataset.silhouettes.reserve(camera_records.size());
  dataset.color_images.reserve(camera_records.size());

  for (const CalibratedImage &record : camera_records) {
    const fs::path image_path =
        findByNameOrStem(config.paths.images_dir, record.image_name);
    cv::Mat image = readImage(image_path, cv::IMREAD_COLOR);
    const fs::path mask_path =
        findByNameOrStem(config.paths.masks_dir, image_path);
    cv::Mat mask = readImage(mask_path, cv::IMREAD_GRAYSCALE);
    cv::threshold(mask, mask, config.foreground_threshold.value_or(1), 255,
                  cv::THRESH_BINARY);

    if (mask.empty() || mask.size() != image.size()) {
      throw std::runtime_error("invalid silhouette for " + image_path.string());
    }

    dataset.cameras.push_back(record.camera);
    dataset.silhouettes.push_back(std::move(mask));
    dataset.color_images.push_back(std::move(image));
  }

  std::cout << "Loaded " << dataset.cameras.size() << " calibrated views"
            << std::endl;
  return dataset;
}
