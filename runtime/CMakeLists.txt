cmake_minimum_required(VERSION 3.6)
set (SOURCE_FILES
        lib.cpp
        lin_check.cpp
        logger.cpp
        pretty_printer.cpp
        scheduler.cpp
        verifying.cpp 
        generators.cpp
)

add_library(runtime STATIC ${SOURCE_FILES})

find_package(Boost REQUIRED  COMPONENTS context)
target_include_directories(runtime PRIVATE include ${gflags_BINARY_DIR}/include ${Boost_INCLUDE_DIRS})
target_link_libraries(runtime PRIVATE gflags::gflags  ${Boost_LIBRARIES})
target_link_options(runtime PRIVATE ${CMAKE_ASAN_FLAGS})
target_compile_options(runtime PRIVATE ${CMAKE_ASAN_FLAGS})

