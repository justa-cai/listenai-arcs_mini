listenai_library_named(system)

listenai_library_sources(
    sys_tick.c
    sys_entry.c
    copy_table.c
    newlib_stubs.c
    printk.c
    ipc_auto_init.c
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
add_subdirectory(sys)

if(DEFINED CONFIG_BOOT
   OR DEFINED CONFIG_BOOT_OTA_PACKAGE
   OR DEFINED CONFIG_BOOT_RECOVERY_API
   OR DEFINED CONFIG_BOOT_WDT_API
   OR DEFINED CONFIG_BOOT_OTA_API)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/uboot/api/CMakeLists.txt)
        add_subdirectory(uboot/api)
    endif()
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (DEFINED CONFIG_BACK_TRACE)
    add_compile_definitions(CFG_BACK_TRACE)
endif()

if(CONFIG_BOOT AND NOT BOOT_STANDALONE_BUILD AND NOT SKIP_STARTUP_BOOT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/boot.cmake)
endif()
