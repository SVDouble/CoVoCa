#include "HelperFunctions.h"

int calculateFlattenedIndex(
    int _size,
    int _row,
    int _column,
    int _depth)
{
    return _row + _size * (_column + _size * _depth);
}

void visualizeSilhouette(
    const cv::Mat &original,
    const cv::Mat &silhouette,
    const std::string &windowName)
{
    if (original.empty() || silhouette.empty())
    {
        std::cerr << "Cannot visualize empty images!" << std::endl;
        return;
    }

    cv::Mat sideBySide;
    cv::hconcat(original, silhouette, sideBySide);

    cv::imshow(windowName, sideBySide);
    cv::waitKey(1);
}

std::vector<cv::Mat> loadImages(std::string _folder)
{
    int i = 0;
    cv::Mat image;
    std::vector<cv::Mat> imageVector;

    for (const auto &entry : fs::directory_iterator(_folder))
    {
        i++;

        image = cv::imread(
            entry.path().string(),
            cv::IMREAD_COLOR);

        std::string extension =
            entry.path().extension().string();

        if (extension == ".png")
        {
            std::cout << "Loaded image no. "
                      << i
                      << " from "
                      << entry.path().filename()
                      << " : "
                      << image.cols << "x"
                      << image.rows
                      << std::endl;

            imageVector.push_back(image);
        }
    }

    return imageVector;
}

std::vector<Camera> loadCameras(const std::string &filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file: " + filename);
    }

    int num_cameras;
    file >> num_cameras;

    std::vector<Camera> cameras;

    for (int i = 0; i < num_cameras; ++i)
    {
        std::string image_name;
        file >> image_name;

        Eigen::Matrix3d K;
        Eigen::Matrix3d R;
        Eigen::Vector3d t;

        // Read K
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                file >> K(row, col);
            }
        }

        // Read R
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                file >> R(row, col);
            }
        }

        // Read t
        for (int row = 0; row < 3; ++row)
        {
            file >> t(row);
        }

        cameras.emplace_back(K, R, t);

        std::cout << "Loaded camera no. " << i << std::endl;
    }

    return cameras;
}