cmake_minimum_required(VERSION 3.20)

project(GaussianSplatting LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Eigen3 REQUIRED)
find_package(spdlog REQUIRED)

add_library(gsplat_voxel
    src/gsplat/voxel/VoxelGrid.cpp
)

target_include_directories(gsplat_voxel PUBLIC
    include
)

target_link_libraries(gsplat_voxel PUBLIC
    Eigen3::Eigen
)

add_executable(voxel_demo
    apps/voxel_grid_demo.cpp
)

target_link_libraries(voxel_demo PRIVATE
    gsplat_voxel
    spdlog::spdlog
)
