#include "ExtractorConfigs.h"
#include "PlanarHomographyExtractor.h"
#include "SilhouetteExtractor.h"
#include "ThresholdExtractor.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <opencv2/aruco.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int findArucoDictionary(const cv::Mat &boardImage) {
  std::vector<int> dictIds = {
      cv::aruco::DICT_4X4_50,        cv::aruco::DICT_4X4_100,
      cv::aruco::DICT_4X4_250,       cv::aruco::DICT_4X4_1000,
      cv::aruco::DICT_5X5_50,        cv::aruco::DICT_5X5_100,
      cv::aruco::DICT_5X5_250,       cv::aruco::DICT_5X5_1000,
      cv::aruco::DICT_6X6_50,        cv::aruco::DICT_6X6_100,
      cv::aruco::DICT_6X6_250,       cv::aruco::DICT_6X6_1000,
      cv::aruco::DICT_7X7_50,        cv::aruco::DICT_7X7_100,
      cv::aruco::DICT_7X7_250,       cv::aruco::DICT_7X7_1000,
      cv::aruco::DICT_ARUCO_ORIGINAL};

  int bestDictId = -1;
  int maxMarkers = 0;

  for (int dictId : dictIds) {
    auto dict = cv::aruco::getPredefinedDictionary(dictId);
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    cv::aruco::ArucoDetector(dict).detectMarkers(boardImage, corners, ids);

    if (ids.size() > maxMarkers) {
      maxMarkers = ids.size();
      bestDictId = dictId;
    }
  }

  if (bestDictId != -1) {
    std::cout << "Best dictionary: ID " << bestDictId << " with " << maxMarkers
              << " markers.\n";
  } else {
    std::cout << "No dictionary detected markers.\n";
  }
  return bestDictId;
}

std::unique_ptr<SilhouetteExtractor>
createPlanarExtractor(const std::string &refPath) {
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
  cv::aruco::ArucoDetector(dict).detectMarkers(
      config.referenceImage, config.referenceCorners, config.referenceIds);

  if (config.referenceIds.empty()) {
    std::cerr << "No markers detected.\n";
    return nullptr;
  }

  config.minMarkersRequired = 4;

  std::cout << "Planar extractor ready: " << config.referenceIds.size()
            << " markers.\n";
  return std::make_unique<PlanarHomographyExtractor>(config);
}

std::unique_ptr<SilhouetteExtractor>
createThresholdExtractor(int thresholdValue) {
  ThresholdConfig config;
  config.diffThreshold = thresholdValue;
  std::cout << "Threshold extractor ready: value = " << thresholdValue << "\n";
  return std::make_unique<ThresholdExtractor>(config);
}

// Helper: show side-by-side comparison in one window
void showSideBySide(const cv::Mat &original, const cv::Mat &mask,
                    const cv::Mat &overlay,
                    const std::string &windowName = "Comparison") {
  // Convert mask to 3-channel for concatenation
  cv::Mat maskColor;
  cv::cvtColor(mask, maskColor, cv::COLOR_GRAY2BGR);

  // Resize all to a common height to keep them manageable
  int targetHeight = 300;
  std::vector<cv::Mat> images = {original, maskColor, overlay};
  std::vector<cv::Mat> resized;
  for (auto &img : images) {
    if (img.empty())
      continue;
    double scale = static_cast<double>(targetHeight) / img.rows;
    int newWidth = static_cast<int>(img.cols * scale);
    cv::Mat resizedImg;
    cv::resize(img, resizedImg, cv::Size(newWidth, targetHeight));
    resized.push_back(resizedImg);
  }

  if (resized.size() < 3)
    return;

  // Concatenate horizontally
  cv::Mat combined;
  cv::hconcat(resized, combined);

  // Show in a resizable window
  cv::namedWindow(windowName, cv::WINDOW_NORMAL);
  cv::imshow(windowName, combined);
  cv::waitKey(0);
  cv::destroyWindow(windowName);
}

void processImage(SilhouetteExtractor &extractor, const std::string &imagePath,
                  bool show = true, bool save = false) {
  cv::Mat image = cv::imread(imagePath);
  if (image.empty()) {
    std::cerr << "ERROR: Failed to load image: " << imagePath << "\n";
    return;
  }

  std::cout << "Processing: " << imagePath << " [" << extractor.name() << "]\n";

  cv::Mat mask = extractor.extract(image);

  // Overlay
  cv::Mat overlay;
  image.copyTo(overlay);
  overlay.setTo(cv::Scalar(0, 0, 255), mask);

  if (show) {
    showSideBySide(image, mask, overlay, "Result: " + extractor.name());
  }

  if (save) {
    std::string outPath = fs::path(imagePath).stem().string() + "_" +
                          extractor.name() + "_mask.png";
    cv::imwrite(outPath, mask);
    std::cout << "Saved: " << outPath << "\n";
  }
}

void processDirectory(SilhouetteExtractor &extractor,
                      const std::string &dirPath, bool show = true,
                      bool save = false) {
  if (!fs::is_directory(dirPath)) {
    std::cerr << "Not a directory: " << dirPath << "\n";
    return;
  }

  std::vector<std::string> imageFiles;
  for (const auto &entry : fs::directory_iterator(dirPath)) {
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
  for (const auto &f : imageFiles) {
    processImage(extractor, f, show, save);
  }
}

int main(int argc, char **argv) {
  // Usage:
  //   Planar:    ./test_silhouette_extraction --method planar <ref_image>
  //   <input> Threshold: ./test_silhouette_extraction --method threshold
  //   <threshold_value> <input>

  if (argc < 4) {
    std::cerr << "Usage:\n";
    std::cerr << "  Planar:   " << argv[0]
              << " --method planar <ref_image> <input>\n";
    std::cerr << "  Threshold:" << argv[0]
              << " --method threshold <threshold_value> <input>\n";
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
    if (!extractor)
      return -1;

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
    if (!extractor)
      return -1;

    if (fs::is_directory(inputPath))
      processDirectory(*extractor, inputPath, true, false);
    else
      processImage(*extractor, inputPath, true, false);
  }

  std::cout << "Done.\n";
  return 0;
}
