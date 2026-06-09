cmake_minimum_required(VERSION 3.16)

project(GaussianSplatting)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
