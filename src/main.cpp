#include <iostream>
#include <Eigen/Dense>

#include "Voxel.h"
#include "VoxelGrid.h"

int main()
{
    int size = 10;
    double step_size = 0.1;

    VoxelGrid voxel_grid(size, step_size);

    std::cout << "Voxelgrid size: " << voxel_grid.getSize() << std::endl;
    std::cout << "Voxelgrid stepsize: " << voxel_grid.getStepSize() << std::endl;

    Voxel test_voxel = voxel_grid.getVoxel(0, 0, 0);

    std::cout << "Voxel cartesian position: " << test_voxel.getCartesianPos() << std::endl;
    std::cout << "Voxel index position: " << test_voxel.getIndexPos() << std::endl;

    return 0;
}