cmake_minimum_required(VERSION 3.16)

project(CoVoCa)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Eigen3_DIR "C:/Users/FengVV/CoVoCa/lib/eigen-5.0.1")
set(OpenCV_DIR "C:/Users/FengVV/opencv")

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(main
    src/main.cpp
    src/SilhouetteExtractor.cpp
    src/Camera.cpp
    src/Voxel.cpp
    src/VoxelGrid.cpp
    src/HelperFunctions.cpp
    src/VoxelCarver.cpp
    src/ArucoPoseEstimator.cpp
)

target_include_directories(main PRIVATE
    include
)

target_link_libraries(main PRIVATE
    Eigen3::Eigen
    ${OpenCV_LIBS}
)
