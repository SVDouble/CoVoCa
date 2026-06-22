#include "ThresholdExtractor.h"

ThresholdExtractor::ThresholdExtractor(const ThresholdConfig &config) : m_config(config){

}

cv::Mat ThresholdExtractor::extract(const cv::Mat &inputImage) {
  if (inputImage.empty()) {
    std::cerr << "Error: Input image is empty!" << std::endl;
    return {};
  }

  cv::Mat grayImage;
  cv::cvtColor(inputImage, grayImage, cv::COLOR_BGR2GRAY);

  cv::Mat binaryImage;
  //pixel > threshold becomes 255 (white), else becomes 0 (black)
  cv::threshold(grayImage, binaryImage, m_config.diffThreshold, 255, cv::THRESH_BINARY);

  return binaryImage;
}

std::string ThresholdExtractor::name() const {
  return "ThresholdExtractor";
}
