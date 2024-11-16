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

<<<<<<< HEAD:src/test/runtime/CMakeLists.txt
target_compile_options(lin_check_test PRIVATE -Wall -Wextra -Werror -pedantic-errors)
=======
add_executable(
        lin_check_dual_test
        lin_check_dual_test.cpp
)
target_compile_options(lin_check_test PUBLIC ${CMAKE_ASAN_FLAGS})
target_link_options(lin_check_test PUBLIC ${CMAKE_ASAN_FLAGS})

target_compile_options(lin_check_test PRIVATE -g -Wall -Wextra -Werror -pedantic-errors)
target_include_directories(lin_check_test PRIVATE ../../runtime/include)
>>>>>>> 43c4393 (erase build logic from verify script):test/runtime/CMakeLists.txt

target_link_libraries(
        lin_check_test
        PRIVATE
        runtime
        mocks
<<<<<<< HEAD:src/test/runtime/CMakeLists.txt
=======
        GTest::gmock
)

target_compile_options(lin_check_dual_test PRIVATE -g -Wall -Wextra -Werror -pedantic-errors)

target_include_directories(lin_check_dual_test PRIVATE ../../verifying/specs)
target_include_directories(lin_check_dual_test PRIVATE ../../runtime/include)


target_link_libraries(
        lin_check_dual_test
        PRIVATE
        runtime
        mocks
        GTest::gmock
>>>>>>> 43c4393 (erase build logic from verify script):test/runtime/CMakeLists.txt
)

link_fuzztest(lin_check_test)
gtest_discover_tests(lin_check_test)
