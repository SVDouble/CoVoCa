cmake_minimum_required(VERSION 3.16)

project(CoVoCa)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/ThresholdExtractor.cpp
    src/PlanarHomographyExtractor.cpp
    src/ColorReconstructor.cpp
)

target_include_directories(main PRIVATE
        include
)

target_link_libraries(main PRIVATE
        Eigen3::Eigen
        ${OpenCV_LIBS}
)

add_executable(test_silhouette_extraction
        testing/test_silhouette_extraction.cpp
        src/PlanarHomographyExtractor.cpp
        src/ThresholdExtractor.cpp
        src/SilhouetteExtractor.cpp
        src/HelperFunctions.cpp
        src/Camera.cpp
)

target_include_directories(test_silhouette_extraction PRIVATE
        include)

target_link_libraries(test_silhouette_extraction PRIVATE
        Eigen3::Eigen
        ${OpenCV_LIBS})

add_executable(test_color_reconstruction
        testing/test_color_reconstruction.cpp
        src/VoxelCarver.cpp
        src/ColorReconstructor.cpp
        src/VoxelGrid.cpp
        src/Voxel.cpp
        src/Camera.cpp
        src/HelperFunctions.cpp
        src/ThresholdExtractor.cpp
)

target_include_directories(test_color_reconstruction PRIVATE
        include
)

target_link_libraries(test_color_reconstruction PRIVATE
        Eigen3::Eigen
        ${OpenCV_LIBS}
)
