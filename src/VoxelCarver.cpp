#include "VoxelCarver.h"

// Constructor
VoxelCarver::VoxelCarver(VoxelGrid _voxel_grid,
                         std::vector<cv::Mat> _silhouette_vector,
                         std::vector<Camera> _camera_vector)
    : m_voxel_grid(std::move(_voxel_grid))
{
    size_t n = std::min(_silhouette_vector.size(),
                        _camera_vector.size());

    m_view_vector.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        View view;
        view.camera = std::move(_camera_vector[i]);
        view.silhouette = std::move(_silhouette_vector[i]);

        m_view_vector.push_back(std::move(view));
    }
}

// Getters
const VoxelGrid &VoxelCarver::getVoxelGrid() const
{
    return m_voxel_grid;
}

const std::vector<View> &VoxelCarver::getViewVector() const
{
    return m_view_vector;
}

void VoxelCarver::carve()
{
    // get all the voxels in the grid
    const auto &voxels = m_voxel_grid.getVoxelGrid(); 
    size_t num_voxels = voxels.size();

    // iterate through each voxel
    for (size_t i = 0; i < num_voxels; ++i)
    {
        // get the current voxel
        Voxel voxel = voxels[i];

        // check if the voxel is carved 
        if (!voxel.getOccupied())
        {
            continue; // skip if it is already carved
        }

        Eigen::Vector3d pos_3d = voxel.getCartesianPos(); //get position of the voxel in 3D space

        bool is_part_of_object = true; // flag to check if the voxel is part of the object


        // iterate through each view 
        for (const auto &view : m_view_vector)
        {
            // project the 3D position of the voxel to 2D coordinates
            auto projected_point = view.camera.projectPoint(pos_3d);

            // get int value 
            int x = static_cast<int>(std::round(projected_point.x));
            int y = static_cast<int>(std::round(projected_point.y));

            // check if the projected point locates in the silhouette bounds
            if (x >= 0 && x < view.silhouette.cols && 
                y >= 0 && y < view.silhouette.rows)
            {
        
            // check if the pixel in the silhouette is black (0)
            if (view.silhouette.at<uchar>(y, x) == 0) // black pixel means it's outside the object
            {
                is_part_of_object = false; // so it is  not part of object
                break;
            }
        }
    }

    if (!is_part_of_object)
    {
        m_voxel_grid.setVoxelOccupied(false, i); // carve the voxel by setting it to unoccupied
    }

   

}