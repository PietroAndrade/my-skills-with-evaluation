cmake_minimum_required(VERSION 3.12)
project(audit_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(GTest REQUIRED)

set(AUDIT_HEADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")

file(GLOB_RECURSE AUDIT_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(audit_tests ${AUDIT_TEST_SOURCES})

target_compile_definitions(audit_tests PRIVATE UNIT_TEST)

target_link_libraries(audit_tests
    PRIVATE
        audit
        GTest::gtest
        GTest::gmock_main
)

target_include_directories(audit_tests
    PRIVATE
        ${AUDIT_HEADERS_DIR}
)

add_test(NAME audit_tests COMMAND audit_tests)
