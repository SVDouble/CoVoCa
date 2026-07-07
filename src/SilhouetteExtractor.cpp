#include "SilhouetteExtractor.h"
#include <iostream>

void SilhouetteExtractor::saveSilhouette(const cv::Mat &silhouette,
                                         const std::string &filename) {

  if (silhouette.empty()) {
    std::cerr << "Warning: Cannot save empty silhouette!" << std::endl;
    return;
  }

  cv::imwrite(filename, silhouette);
  std::cout << "Saved silhouette to: " << filename << std::endl;
}
