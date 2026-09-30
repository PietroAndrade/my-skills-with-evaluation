cmake_minimum_required(VERSION 3.16)
project(parallel_downloads CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(parallel_downloads main.cpp)
target_link_libraries(parallel_downloads PRIVATE Threads::Threads)
