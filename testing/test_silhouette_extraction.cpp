#include "SilhouetteExtractor.h"
#include "PlanarHomographyExtractor.h"
#include "ThresholdExtractor.h"
#include "ExtractorConfigs.h"

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

int findArucoDictionary(const cv::Mat& boardImage) {
  std::vector<int> dictIds = {
      cv::aruco::DICT_4X4_50, cv::aruco::DICT_4X4_100,
      cv::aruco::DICT_4X4_250, cv::aruco::DICT_4X4_1000,
      cv::aruco::DICT_5X5_50, cv::aruco::DICT_5X5_100,
      cv::aruco::DICT_5X5_250, cv::aruco::DICT_5X5_1000,
      cv::aruco::DICT_6X6_50, cv::aruco::DICT_6X6_100,
      cv::aruco::DICT_6X6_250, cv::aruco::DICT_6X6_1000,
      cv::aruco::DICT_7X7_50, cv::aruco::DICT_7X7_100,
      cv::aruco::DICT_7X7_250, cv::aruco::DICT_7X7_1000,
      cv::aruco::DICT_ARUCO_ORIGINAL
  };

  std::cout << "Scanning for Aruco dictionary...\n";
  for (int dictId : dictIds) {
    auto dict = cv::aruco::getPredefinedDictionary(dictId);
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    cv::aruco::detectMarkers(boardImage, dict, corners, ids);
    if (!ids.empty()) {
      std::cout << "Found dictionary ID: " << dictId
                << " with " << ids.size() << " markers.\n";
      return dictId;
    }
  }
  std::cout << "No dictionary detected.\n";
  return -1;
}

std::unique_ptr<SilhouetteExtractor> createPlanarExtractor(
    const std::string& refPath) {
  PlanarHomographyConfig config;
  config.referenceImage = cv::imread(refPath);
  if (config.referenceImage.empty()) {
    std::cerr << "Failed to load reference: " << refPath << "\n";
    return nullptr;
  }

  int dictId = findArucoDictionary(config.referenceImage);
  if (dictId == -1) {
    std::cerr << "No markers in reference.\n";
    return nullptr;
  }
  config.arucoDictionaryId = dictId;

  auto dict = cv::aruco::getPredefinedDictionary(dictId);
  cv::aruco::detectMarkers(config.referenceImage, dict,
                           config.referenceCorners, config.referenceIds);

  if (config.referenceIds.empty()) {
    std::cerr << "No markers detected.\n";
    return nullptr;
  }

  config.diffThreshold = 30;
  config.useShadowDetection = false;
  config.shadowBrightnessRatio = 0.7f;
  config.minMarkersRequired = 4;

  std::cout << "Planar extractor ready: " << config.referenceIds.size() << " markers.\n";
  return std::make_unique<PlanarHomographyExtractor>(config);
}

std::unique_ptr<SilhouetteExtractor> createThresholdExtractor(int thresholdValue) {
  ThresholdConfig config;
  config.diffThreshold = thresholdValue;
  std::cout << "Threshold extractor ready: value = " << thresholdValue << "\n";
  return std::make_unique<ThresholdExtractor>(config);
}

void processImage(SilhouetteExtractor& extractor,
                  const std::string& imagePath,
                  bool show = true,
                  bool save = false) {
  cv::Mat image = cv::imread(imagePath);
  if (image.empty()) {
    std::cerr << "Failed to load: " << imagePath << "\n";
    return;
  }

  std::cout << "Processing: " << imagePath << " [" << extractor.name() << "]\n";

  cv::Mat mask = extractor.extract(image);

  // Visualize overlay
  cv::Mat overlay;
  image.copyTo(overlay);
  overlay.setTo(cv::Scalar(0, 0, 255), mask);

  if (show) {
    cv::imshow("Original", image);
    cv::imshow("Mask: " + extractor.name(), mask);
    cv::imshow("Overlay: " + extractor.name(), overlay);
    cv::waitKey(0);
  }

  if (save) {
    std::string outPath = fs::path(imagePath).stem().string()
                          + "_" + extractor.name() + "_mask.png";
    cv::imwrite(outPath, mask);
    std::cout << "Saved: " << outPath << "\n";
  }
}

void processDirectory(SilhouetteExtractor& extractor,
                      const std::string& dirPath,
                      bool show = true,
                      bool save = false) {
  if (!fs::is_directory(dirPath)) {
    std::cerr << "Not a directory: " << dirPath << "\n";
    return;
  }

  std::vector<std::string> imageFiles;
  for (const auto& entry : fs::directory_iterator(dirPath)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
        imageFiles.push_back(entry.path().string());
      }
    }
  }

  if (imageFiles.empty()) {
    std::cerr << "No images found in directory.\n";
    return;
  }

  std::cout << "Found " << imageFiles.size() << " images.\n";
  for (const auto& f : imageFiles) {
    processImage(extractor, f, show, save);
  }
}

int main(int argc, char** argv) {
  // Usage:
  //   Planar:    ./test_silhouette_extraction --method planar <ref_image> <input>
  //   Threshold: ./test_silhouette_extraction --method threshold <threshold_value> <input>

  if (argc < 4) {
    std::cerr << "Usage:\n";
    std::cerr << "  Planar:   " << argv[0] << " --method planar <ref_image> <input>\n";
    std::cerr << "  Threshold:" << argv[0] << " --method threshold <threshold_value> <input>\n";
    std::cerr << "\nNote: <input> can be an image file or a directory.\n";
    return -1;
  }

  std::string method = argv[1];
  std::unique_ptr<SilhouetteExtractor> extractor;
  std::string inputPath;

  if (method == "--method" && std::string(argv[2]) == "planar") {
    if (argc < 5) {
      std::cerr << "Planar needs: <ref_image> <input>\n";
      return -1;
    }
    std::string refPath = argv[3];
    inputPath = argv[4];
    extractor = createPlanarExtractor(refPath);
    if (!extractor) return -1;

    if (fs::is_directory(inputPath))
      processDirectory(*extractor, inputPath, true, false);
    else
      processImage(*extractor, inputPath, true, false);
  }

  else if (method == "--method" && std::string(argv[2]) == "threshold") {
    if (argc < 5) {
      std::cerr << "Threshold needs: <threshold_value> <input>\n";
      return -1;
    }
    int thresholdVal = std::stoi(argv[3]);
    inputPath = argv[4];

    extractor = createThresholdExtractor(thresholdVal);
    if (!extractor) return -1;

    if (fs::is_directory(inputPath))
      processDirectory(*extractor, inputPath, true, false);
    else
      processImage(*extractor, inputPath, true, false);
  }

  std::cout << "Done.\n";
  return 0;
}