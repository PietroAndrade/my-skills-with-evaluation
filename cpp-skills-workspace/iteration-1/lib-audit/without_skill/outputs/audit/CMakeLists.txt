cmake_minimum_required(VERSION 3.15)
project(audit CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(audit STATIC)

target_include_directories(audit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(audit
    PUBLIC
        logger
        filter_dtos
)

add_subdirectory(test)
