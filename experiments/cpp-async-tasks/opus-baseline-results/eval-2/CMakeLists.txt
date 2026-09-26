cmake_minimum_required(VERSION 3.16)
project(async_fan_out LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

foreach(target broken_loop fixed_async fan_out_gather thread_pool)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()
