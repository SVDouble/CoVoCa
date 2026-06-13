#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class SilhouetteExtractor {
public:
    SilhouetteExtractor(int thresholdValue = 30);

    cv::Mat extract(const cv::Mat& inputImage);

    void saveSilhouette(const cv::Mat& silhouette, const std::string& filename);

    void setThreshold(int value);

    int getThreshold();

private:
    int thresholdValue_;
};
