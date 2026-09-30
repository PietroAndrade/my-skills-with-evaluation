cmake_minimum_required(VERSION 3.15)
project(audit CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AUDIT_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(AUDIT_SOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

message("[audit] path to headers: ${AUDIT_HEADERS_DIR}")
message("[audit] path to sources: ${AUDIT_SOURCES_DIR}")

file(GLOB_RECURSE AUDIT_HEADERS "${AUDIT_HEADERS_DIR}/*.h")
file(GLOB_RECURSE AUDIT_SOURCES "${AUDIT_SOURCES_DIR}/*.cpp")

add_library(audit
    STATIC
        ${AUDIT_SOURCES}
        ${AUDIT_HEADERS}
)

target_include_directories(audit
    PUBLIC
        $<BUILD_INTERFACE:${AUDIT_HEADERS_DIR}>
)

target_link_libraries(audit
    PUBLIC
        logger
        filter_dtos
)

set_target_properties(audit
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS audit)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
