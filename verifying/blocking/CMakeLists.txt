set (SOURCE_TARGET_LIST
    mutexed_register.cpp
    simple_mutex.cpp
    nonlinear_mutex.cpp
)

# set (TRAP_SYSCALLS_PATH ${CMAKE_BINARY_DIR}/pintool/libTrapSyscalls.so CACHE INTERNAL "trap syscalls")

foreach(source_name ${SOURCE_TARGET_LIST})
    get_filename_component(target ${source_name} NAME_WE)
    verify_target(${target})
    add_dependencies(${target} preload)
    # add_dependencies(${target} TrapSyscalls)
    # message(NOTICE "${PIN_EXE} -t ${TRAP_SYSCALLS_PATH} -- ${CMAKE_BINARY_DIR}/verifying/blocking/${target} --syscall_trap --rounds 1000")
endforeach(source_name ${SOURCE_TARGET_LIST})
