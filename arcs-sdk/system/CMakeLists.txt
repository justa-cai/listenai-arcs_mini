listenai_library_named(system)

listenai_library_sources(
    sys_tick.c
    sys_entry.c
    copy_table.c
    newlib_stubs.c
    printk.c
)

add_subdirectory(init)
add_subdirectory(console)
add_subdirectory(backtrace)
add_subdirectory(cpp)
add_subdirectory(ringbuf)
add_subdirectory(heap)
add_subdirectory(workq)
add_subdirectory(log)
add_subdirectory(os)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (DEFINED CONFIG_BACK_TRACE)
    add_compile_definitions(CFG_BACK_TRACE)
endif()

if (DEFINED CONFIG_BOOT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/boot.cmake)
endif()
