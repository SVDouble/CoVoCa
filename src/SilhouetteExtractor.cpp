#include "SilhouetteExtractor.h"
#include <iostream>

SilhouetteExtractor::SilhouetteExtractor(int thresholdValue) {

    thresholdValue_ = thresholdValue;
}

cv::Mat SilhouetteExtractor::extract(const cv::Mat &inputImage) {

    if (inputImage.empty()) {
        std::cerr << "Error: Input image is empty!" << std::endl;
        return cv::Mat();
    }

    cv::Mat grayImage;
    cv::cvtColor(inputImage, grayImage, cv::COLOR_BGR2GRAY);

    cv::Mat binaryImage;
    //pixel > threshold becomes 255 (white), else becomes 0 (black)
    cv::threshold(grayImage, binaryImage, thresholdValue_, 255, cv::THRESH_BINARY);

    return binaryImage;
}

void SilhouetteExtractor::saveSilhouette(const cv::Mat &silhouette, const std::string &filename) {

    if (silhouette.empty()) {
        std::cerr << "Warning: Cannot save empty silhouette!" << std::endl;
        return;
    }

    cv::imwrite(filename, silhouette);
    std::cout << "Saved silhouette to: " << filename << std::endl;
}

void SilhouetteExtractor::setThreshold(int value) {

    thresholdValue_ = std::clamp(value, 0, 255);
    std::cout << "Threshold set to: " << thresholdValue_ << std::endl;
}

int SilhouetteExtractor::getThreshold() {

    return thresholdValue_;
}
