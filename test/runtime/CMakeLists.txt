cmake_minimum_required(VERSION 3.6)

enable_testing()

add_library(mocks stackfulltask_mock.h)

target_link_libraries(
        mocks
        PRIVATE
        runtime
        GTest::gmock_main
)

add_executable(
        lin_check_test
        lin_check_test.cpp
)

target_compile_options(lin_check_test PRIVATE ${CMAKE_ASAN_FLAGS})
target_link_options(lin_check_test PRIVATE ${CMAKE_ASAN_FLAGS})

target_include_directories(lin_check_test PRIVATE ../../runtime/include)

target_link_libraries(
        lin_check_test
        PRIVATE
        runtime
        mocks
        GTest::gmock
)

link_fuzztest(lin_check_test)
gtest_discover_tests(lin_check_test)
