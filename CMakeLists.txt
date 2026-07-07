cmake_minimum_required(VERSION 3.20)

project(CoVoCa)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Eigen3 REQUIRED)
find_package(OpenCV 4.7 REQUIRED COMPONENTS
    aruco
    calib3d
    core
    highgui
    imgcodecs
    imgproc
)
find_package(reflectcpp CONFIG QUIET)

if(NOT TARGET reflectcpp::reflectcpp AND NOT TARGET reflectcpp)
    set(REFLECTCPP_YAML ON CACHE BOOL "" FORCE)
    set(REFLECTCPP_USE_VCPKG OFF CACHE BOOL "" FORCE)
    set(REFLECTCPP_INSTALL OFF CACHE BOOL "" FORCE)
    set(REFLECTCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(REFLECTCPP_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        reflectcpp
        GIT_REPOSITORY https://github.com/getml/reflect-cpp.git
        GIT_TAG v0.25.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(reflectcpp)
endif()

if(TARGET reflectcpp::reflectcpp)
    set(COVOCA_REFLECTCPP_TARGET reflectcpp::reflectcpp)
elseif(TARGET reflectcpp)
    set(COVOCA_REFLECTCPP_TARGET reflectcpp)
else()
    message(FATAL_ERROR "reflectcpp target was not created")
endif()

add_executable(main
    src/main.cpp
    src/DatasetLoader.cpp
    src/Camera.cpp
    src/Voxel.cpp
    src/VoxelGrid.cpp
    src/VoxelCarvingConfig.cpp
    src/HelperFunctions.cpp
    src/VoxelCarver.cpp
    src/ColorReconstructor.cpp
)

target_include_directories(main PRIVATE
        include
)

target_link_libraries(main PRIVATE
        Eigen3::Eigen
        ${OpenCV_LIBS}
        ${COVOCA_REFLECTCPP_TARGET}
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
