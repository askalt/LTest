cmake_minimum_required(VERSION 3.6)
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
project(lintest)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# TODO(kmitkin): require to understand, what is it considered to be "optimized" build
# SET(CMAKE_CXX_FLAGS_RELEASE "???")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb3 -O0 -coverage -fno-omit-frame-pointer")
# SET(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}")

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Debug mode ON")
    SET(CMAKE_ASAN_FLAGS -fsanitize=address -fsanitize=undefined -DADDRESS_SANITIZER)
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

add_subdirectory(third_party)

include(GoogleTest)

fuzztest_setup_fuzzing_flags()
enable_testing()

add_subdirectory(codegen)
add_subdirectory(runtime)

add_subdirectory(test)
add_subdirectory(verifying)
