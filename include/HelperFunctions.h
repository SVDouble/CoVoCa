#pragma once

#include <iostream>
#include <Eigen/Dense>
#include <opencv2/opencv.hpp>

// Flattened index to access voxelgrid matrix
int calculateFlattenedIndex(int _size, int _row, int _coloumn, int _depth)
{
    return _row + _size * (_coloumn + _size * _depth);
}

void visualizeSilhouette(const cv::Mat& original, const cv::Mat& silhouette,
                         const std::string& windowName = "Silhouette Check") {

    if (original.empty() || silhouette.empty()) {
        std::cerr << "Cannot visualize empty images!" << std::endl;
        return;
    }

    cv::Mat sideBySide;
    cv::hconcat(original,silhouette, sideBySide);

    cv::imshow(windowName, sideBySide);
    cv::waitKey(1);
}
