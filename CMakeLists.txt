cmake_minimum_required(VERSION 3.20)

project(GaussianSplatting LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Eigen3 REQUIRED)

add_executable(main
    src/main.cpp
)

target_include_directories(main PRIVATE
    include
)

target_link_libraries(main PRIVATE
    Eigen3::Eigen
)
