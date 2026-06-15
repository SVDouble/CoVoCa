#include "HelperFunctions.h"

int calculateFlattenedIndex(const Eigen::Vector3i &size,
                            int x,
                            int y,
                            int z)
{
    return x + size.x() * (y + size.y() * z);
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
    std::vector<std::filesystem::path> files;

    // collect all png files
    for (const auto &entry : std::filesystem::directory_iterator(_folder))
    {
        if (entry.path().extension() == ".png")
        {
            files.push_back(entry.path());
        }
    }

    // sort by filename (lexicographical order),
    // important to load in same order as cameras since index in vector detemines which camera is associated
    std::sort(files.begin(), files.end());

    std::vector<cv::Mat> imageVector;

    int i = 0;

    for (const auto &path : files)
    {
        cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);

        if (image.empty())
        {
            std::cerr << "Failed to load: " << path << std::endl;
            continue;
        }

        std::cout << "Loaded image no. "
                  << i
                  << " from "
                  << path.filename()
                  << " : "
                  << image.cols << "x"
                  << image.rows
                  << std::endl;

        imageVector.push_back(image);
        i++;
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

        std::cout << "Loaded camera no. " << i << " associated with image " << image_name << std::endl;
    }

    return cameras;
}