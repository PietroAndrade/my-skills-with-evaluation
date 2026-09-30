cmake_minimum_required(VERSION 3.12)
project(audit_tests CXX)

find_package(GTest REQUIRED)

add_executable(audit_tests)

target_link_libraries(audit_tests
    PRIVATE
        audit
        GTest::gtest_main
        GTest::gmock
)

add_test(NAME audit_tests COMMAND audit_tests)
