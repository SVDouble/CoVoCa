cmake_minimum_required(VERSION 3.20)

project(GaussianSplatting LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GSPLAT_ENABLE_WARNINGS "Enable project compiler warnings." ON)
option(GSPLAT_ENABLE_SANITIZERS "Enable AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer." OFF)

find_package(Eigen3 REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS
    aruco
    calib3d
    core
    imgcodecs
    objdetect
)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(CLI11 CONFIG QUIET)
find_package(reflectcpp CONFIG QUIET)

if(NOT TARGET CLI11::CLI11)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.5.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(CLI11)
endif()

if(NOT TARGET reflectcpp::reflectcpp)
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

if(TARGET reflectcpp)
    get_target_property(GSPLAT_REFLECTCPP_INCLUDE_DIRS reflectcpp INTERFACE_INCLUDE_DIRECTORIES)
    if(GSPLAT_REFLECTCPP_INCLUDE_DIRS)
        set_target_properties(reflectcpp PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${GSPLAT_REFLECTCPP_INCLUDE_DIRS}"
        )
    endif()
elseif(TARGET reflectcpp::reflectcpp)
    get_target_property(GSPLAT_REFLECTCPP_INCLUDE_DIRS reflectcpp::reflectcpp INTERFACE_INCLUDE_DIRECTORIES)
    if(GSPLAT_REFLECTCPP_INCLUDE_DIRS)
        set_target_properties(reflectcpp::reflectcpp PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${GSPLAT_REFLECTCPP_INCLUDE_DIRS}"
        )
    endif()
endif()

add_library(gsplat_project_options INTERFACE)

if(GSPLAT_ENABLE_WARNINGS)
    target_compile_options(gsplat_project_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endif()

if(GSPLAT_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(gsplat_project_options INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(gsplat_project_options INTERFACE
            -fsanitize=address,undefined
        )
    else()
        message(WARNING "GSPLAT_ENABLE_SANITIZERS is only configured for GCC/Clang-style compilers.")
    endif()
endif()

add_library(gsplat_config INTERFACE)

target_include_directories(gsplat_config INTERFACE
    include
)

target_link_libraries(gsplat_config INTERFACE
    reflectcpp::reflectcpp
    gsplat_project_options
)

add_library(gsplat_voxel
    src/gsplat/voxel/VoxelGrid.cpp
)

target_include_directories(gsplat_voxel PUBLIC
    include
)

target_link_libraries(gsplat_voxel PUBLIC
    Eigen3::Eigen
    gsplat_project_options
)

add_executable(voxel_demo
    apps/voxel_grid_demo.cpp
)

target_link_libraries(voxel_demo PRIVATE
    gsplat_voxel
    gsplat_project_options
    spdlog::spdlog
)

add_library(gsplat_calibration
    src/gsplat/calibration/ArucoCalibration.cpp
    src/gsplat/calibration/CalibrationConfig.cpp
    src/gsplat/calibration/CalibrationResult.cpp
)

target_include_directories(gsplat_calibration PUBLIC
    include
)

target_link_libraries(gsplat_calibration
    PUBLIC
        Eigen3::Eigen
    PRIVATE
        ${OpenCV_LIBS}
        gsplat_config
        gsplat_project_options
        spdlog::spdlog
)

add_executable(aruco_calibrate
    apps/aruco_calibrate.cpp
)

target_link_libraries(aruco_calibrate PRIVATE
    CLI11::CLI11
    gsplat_calibration
    gsplat_project_options
    spdlog::spdlog
)
