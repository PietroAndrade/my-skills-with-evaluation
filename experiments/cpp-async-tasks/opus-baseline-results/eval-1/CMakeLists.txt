cmake_minimum_required(VERSION 3.16)
project(compute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(compute src/parallel_sum.cpp)
target_include_directories(compute PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(compute PUBLIC Threads::Threads)

add_executable(compute_demo src/main.cpp)
target_link_libraries(compute_demo PRIVATE compute)
