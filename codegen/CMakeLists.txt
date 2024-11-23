include_directories(include)

find_package(LLVM REQUIRED CONFIG)

include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

llvm_add_library(YieldPass 
    SHARED 
    PARTIAL_SOURCES_INTENDED 
    yieldpass.cpp)