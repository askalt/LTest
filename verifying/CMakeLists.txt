include_directories(lib)
include_directories(specs)

set (PASS YieldPass)
set (PASS_PATH ${CMAKE_BINARY_DIR}/codegen/lib${PASS}.so)

function(verify_target target)
    add_executable(${target} ${source_name})
    add_dependencies(${target} runtime plugin_pass)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_options(${target} PRIVATE ${CMAKE_ASAN_FLAGS})
    target_compile_options(${target} PRIVATE -fpass-plugin=${PASS_PATH} ${CMAKE_ASAN_FLAGS})
    target_link_libraries(${target} PRIVATE runtime ${PASS} gflags)
endfunction()

add_subdirectory(targets)
add_subdirectory(blocking)
