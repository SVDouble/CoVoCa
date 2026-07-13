#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <cmath>

namespace fs = std::filesystem;

double calculate_iou(const cv::Mat& mask1, const cv::Mat& mask2) {
    cv::Mat intersection_mask, union_mask;
    cv::bitwise_and(mask1, mask2, intersection_mask);
    cv::bitwise_or(mask1, mask2, union_mask);

    double intersection_area = cv::countNonZero(intersection_mask);
    double union_area = cv::countNonZero(union_mask);

    if (union_area == 0) {
        return (intersection_area == 0) ? 1.0 : 0.0;
    }
    return intersection_area / union_area;
}

double calculate_psnr(const cv::Mat& img1, const cv::Mat& img2, const cv::Mat& mask) {
    if (cv::countNonZero(mask) == 0) {
        return 0.0;
    }

    cv::Mat diff;
    cv::absdiff(img1, img2, diff);
    diff.convertTo(diff, CV_32F);

    cv::Mat squared_diff;
    cv::multiply(diff, diff, squared_diff);

    cv::Scalar mean_sq = cv::mean(squared_diff, mask);
    double mse = (mean_sq[0] + mean_sq[1] + mean_sq[2]) / 3.0;

    if (mse == 0) {
        return 100.0;
    }

    double max_pixel = 255.0;
    return 20.0 * std::log10(max_pixel / std::sqrt(mse));
}

int main() {
    // you might change your file here
    fs::path renders_base_dir = "renders/cat";
    fs::path gt_images_dir = "datasets (2)/cat/images";
    fs::path gt_masks_dir = "datasets (2)/cat/masks";

    if (!fs::exists(renders_base_dir)) {
        std::cerr << "Error: " << renders_base_dir << " not found.\n";
        return 1;
    }

    std::vector<fs::path> methods;
    for (const auto& entry : fs::directory_iterator(renders_base_dir)) {
        if (entry.is_directory()) {
            methods.push_back(entry.path());
        }
    }

    std::cout << "Starting evaluation...\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& method_dir : methods) {
        std::string method_name = method_dir.filename().string();

        double total_iou = 0.0;
        double total_psnr = 0.0;
        int valid_frames = 0;

        for (const auto& entry : fs::directory_iterator(method_dir)) {
            if (entry.path().extension() == ".png") {
                std::string file_name_stem = entry.path().stem().string();

                fs::path gt_img_path = gt_images_dir / (file_name_stem + ".jpg");
                fs::path gt_mask_path = gt_masks_dir / (file_name_stem + ".png");

                if (!fs::exists(gt_img_path) || !fs::exists(gt_mask_path)) {
                    continue;
                }

                cv::Mat render_img = cv::imread(entry.path().string(), cv::IMREAD_UNCHANGED);
                cv::Mat gt_img = cv::imread(gt_img_path.string());
                cv::Mat gt_mask_img = cv::imread(gt_mask_path.string(), cv::IMREAD_GRAYSCALE);

                if (render_img.empty() || gt_img.empty() || gt_mask_img.empty()) {
                    continue;
                }

                if (render_img.size() != gt_img.size()) {
                    cv::resize(render_img, render_img, gt_img.size());
                }

                // Flip horizontally
                cv::flip(render_img, render_img, 1);

                cv::Mat gt_mask_bool;
                cv::threshold(gt_mask_img, gt_mask_bool, 127, 255, cv::THRESH_BINARY);

                cv::Mat render_mask_bool;
                cv::Mat render_rgb;

                if (render_img.channels() == 4) {
                    cv::cvtColor(render_img, render_rgb, cv::COLOR_BGRA2BGR);

                    std::vector<cv::Mat> channels;
                    cv::split(render_img, channels);
                    cv::Mat alpha = channels[3];

                    double min_val, max_val;
                    cv::minMaxLoc(alpha, &min_val, &max_val);

                    if (min_val < 255) {
                        cv::threshold(alpha, render_mask_bool, 127, 255, cv::THRESH_BINARY);
                    } else {
                        cv::Mat gray;
                        cv::cvtColor(render_rgb, gray, cv::COLOR_BGR2GRAY);
                        cv::threshold(gray, render_mask_bool, 10, 255, cv::THRESH_BINARY);
                    }
                } else {
                    render_rgb = render_img;
                    cv::Mat gray;
                    cv::cvtColor(render_rgb, gray, cv::COLOR_BGR2GRAY);
                    cv::threshold(gray, render_mask_bool, 10, 255, cv::THRESH_BINARY);
                }

                double iou = calculate_iou(render_mask_bool, gt_mask_bool);
                double psnr = calculate_psnr(render_rgb, gt_img, gt_mask_bool);

                total_iou += iou;
                total_psnr += psnr;
                valid_frames++;
            }
        }

        if (valid_frames > 0) {
            double avg_iou = total_iou / valid_frames;
            double avg_psnr = total_psnr / valid_frames;

            std::cout << "Method: " << method_name << "\n";
            std::cout << "  Frames Evaluated : " << valid_frames << "\n";
            std::cout << "  Silhouette (IoU) : " << avg_iou << "\n";
            std::cout << "  Color (PSNR)     : " << avg_psnr << " dB\n";
            std::cout << std::string(50, '-') << "\n";
        }
    }

    return 0;
}
