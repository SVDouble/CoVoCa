cmake_minimum_required(VERSION 3.20)

project(GaussianSplatting LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GSPLAT_ENABLE_WARNINGS "Enable project compiler warnings." ON)
option(GSPLAT_ENABLE_SANITIZERS "Enable AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer." OFF)

find_package(Eigen3 REQUIRED)
find_package(spdlog REQUIRED)

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
