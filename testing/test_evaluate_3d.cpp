#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>

struct Vertex {
    float x, y, z;
    float r, g, b;
};

// Simple ASCII PLY parser
bool read_ascii_ply(const std::string& filename, std::vector<Vertex>& vertices) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << "\n";
        return false;
    }

    std::string line;
    int vertex_count = 0;
    bool in_header = true;
    bool has_color = false;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (in_header) {
            if (token == "element") {
                std::string type;
                iss >> type;
                if (type == "vertex") {
                    iss >> vertex_count;
                }
            } else if (token == "property") {
                std::string type, name;
                iss >> type >> name;
                if (name == "red" || name == "r") has_color = true;
            } else if (token == "end_header") {
                in_header = false;
                vertices.reserve(vertex_count);
            }
        } else {
            Vertex v = {0, 0, 0, 0, 0, 0};
            if (has_color) {
                // Assuming format: x y z nx ny nz r g b OR x y z r g b
                // Let's do a simple parse: read first 3 as xyz, then if line has >=6 numbers, last 3 are rgb
                std::vector<float> values;
                values.push_back(std::stof(token));
                float val;
                while (iss >> val) values.push_back(val);
                
                v.x = values[0]; v.y = values[1]; v.z = values[2];
                if (values.size() >= 6) {
                    v.b = values[values.size() - 1];
                    v.g = values[values.size() - 2];
                    v.r = values[values.size() - 3];
                }
            } else {
                v.x = std::stof(token);
                iss >> v.y >> v.z;
            }
            vertices.push_back(v);
        }
    }
    return true;
}

cv::Mat vertices_to_mat(const std::vector<Vertex>& vertices) {
    cv::Mat mat(vertices.size(), 3, CV_32F);
    for (size_t i = 0; i < vertices.size(); ++i) {
        mat.at<float>(i, 0) = vertices[i].x;
        mat.at<float>(i, 1) = vertices[i].y;
        mat.at<float>(i, 2) = vertices[i].z;
    }
    return mat;
}

// Compute one-way Chamfer distance and Color MSE
void compute_distances(const std::vector<Vertex>& src, const std::vector<Vertex>& tgt, 
                       double& out_chamfer, double& out_color_mse) {
    if (src.empty() || tgt.empty()) return;

    cv::Mat src_mat = vertices_to_mat(src);
    cv::Mat tgt_mat = vertices_to_mat(tgt);

    // Build KD-Tree on target
    cv::flann::Index kd_tree(tgt_mat, cv::flann::KDTreeIndexParams(4));
    
    cv::Mat indices(src_mat.rows, 1, CV_32S);
    cv::Mat dists(src_mat.rows, 1, CV_32F);
    
    // Find nearest neighbor for each point in src
    kd_tree.knnSearch(src_mat, indices, dists, 1, cv::flann::SearchParams(32));

    double total_dist = 0.0;
    double total_color_mse = 0.0;

    for (int i = 0; i < src_mat.rows; ++i) {
        float squared_dist = dists.at<float>(i, 0);
        total_dist += std::sqrt(squared_dist); // L2 distance

        int nn_idx = indices.at<int>(i, 0);
        const Vertex& p_src = src[i];
        const Vertex& p_tgt = tgt[nn_idx];

        double r_diff = p_src.r - p_tgt.r;
        double g_diff = p_src.g - p_tgt.g;
        double b_diff = p_src.b - p_tgt.b;
        total_color_mse += (r_diff*r_diff + g_diff*g_diff + b_diff*b_diff) / 3.0;
    }

    out_chamfer = total_dist / src.size();
    out_color_mse = total_color_mse / src.size();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <reconstructed.ply> <ground_truth.ply>\n";
        return 1;
    }

    std::string recon_file = argv[1];
    std::string gt_file = argv[2];

    std::vector<Vertex> recon_verts, gt_verts;

    std::cout << "Loading " << recon_file << "...\n";
    if (!read_ascii_ply(recon_file, recon_verts)) return 1;

    std::cout << "Loading " << gt_file << "...\n";
    if (!read_ascii_ply(gt_file, gt_verts)) return 1;

    std::cout << "Reconstructed: " << recon_verts.size() << " vertices.\n";
    std::cout << "Ground Truth:  " << gt_verts.size() << " vertices.\n";
    
    if (recon_verts.empty() || gt_verts.empty()) {
        std::cerr << "One or both point clouds are empty.\n";
        return 1;
    }

    std::cout << "Computing KD-Tree nearest neighbors...\n";

    double chamfer_R2G = 0, color_mse_R2G = 0;
    compute_distances(recon_verts, gt_verts, chamfer_R2G, color_mse_R2G);

    double chamfer_G2R = 0, color_mse_G2R = 0;
    compute_distances(gt_verts, recon_verts, chamfer_G2R, color_mse_G2R);

    // Symmetric Chamfer Distance
    double chamfer_dist = (chamfer_R2G + chamfer_G2R) / 2.0;
    double color_mse = (color_mse_R2G + color_mse_G2R) / 2.0;
    
    double color_psnr = (color_mse > 0) ? 20.0 * std::log10(255.0 / std::sqrt(color_mse)) : 100.0;

    std::cout << std::string(50, '-') << "\n";
    std::cout << "3D Evaluation Results:\n";
    std::cout << "Chamfer Distance (Shape Error) : " << chamfer_dist << " (Lower is better)\n";
    std::cout << "Color Error (PSNR)           : " << color_psnr << " dB (Higher is better)\n";
    std::cout << std::string(50, '-') << "\n";

    return 0;
}
