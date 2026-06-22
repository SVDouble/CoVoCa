#pragma once

#include <opencv2/opencv.hpp>
#include <string>

class SilhouetteExtractor {
public:
    virtual ~SilhouetteExtractor() = default;

    virtual cv::Mat extract(const cv::Mat& inputImage) = 0;

    static void saveSilhouette(const cv::Mat& silhouette, const std::string& filename);

    virtual std::string name() const = 0;
};
