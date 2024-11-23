set (PASS YieldPass)

set (SOURCE_TARGET_LIST
    atomic_register.cpp
    deadlock.cpp
    fast_queue.cpp
    mutex_queue.cpp
    race_register.cpp
    nonlinear_queue.cpp
)

# https://discourse.cmake.org/t/how-to-add-a-dependency-on-a-compiler-plugin/3077/4
add_custom_target(plugin_pass
    COMMAND ${CMAKE_COMMAND} -E echo "Plugin dependency set"
)
add_dependencies(plugin_pass ${PASS})

set (PASS_PATH ${CMAKE_BINARY_DIR}/codegen/lib${PASS}.so)

foreach(source_name ${SOURCE_TARGET_LIST})
    get_filename_component(target ${source_name} NAME_WE)
    add_executable(${target} ${source_name})
    add_dependencies(${target} runtime plugin_pass)
    target_include_directories(${target} PRIVATE ${gflags_BINARY_DIR}/include)
    target_link_options(${target} PRIVATE ${CMAKE_ASAN_FLAGS})
    target_compile_options(${target} PRIVATE -fpass-plugin=${PASS_PATH} ${CMAKE_ASAN_FLAGS})
    target_link_libraries(${target} PRIVATE runtime ${PASS})
endforeach(source_name ${SOURCE_TARGET_LIST})
