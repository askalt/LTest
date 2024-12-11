cmake_minimum_required(VERSION 3.6)

set (SOURCE_FILES
        lib.cpp
        lin_check.cpp
        logger.cpp
        pretty_printer.cpp
        scheduler.cpp
        verifying.cpp 
        generators.cpp
        syscall_trap.cpp
)

add_library(runtime SHARED ${SOURCE_FILES})
target_include_directories(runtime PRIVATE include)
# target_include_directories(runtime PRIVATE include ${gflags_BINARY_DIR}/include)
target_link_libraries(runtime PRIVATE gflags)
target_link_options(runtime PRIVATE ${CMAKE_ASAN_FLAGS})
target_compile_options(runtime PRIVATE ${CMAKE_ASAN_FLAGS})
