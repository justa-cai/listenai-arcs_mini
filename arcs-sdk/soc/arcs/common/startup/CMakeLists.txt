set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})

enable_language(ASM)

listenai_library_named(startup)

listenai_library_sources(
    backtrace_arch.c
    boot_watchdog.c
    cache_ap.c
    freertos_hook.c
    startup.S
    system.c
    soc.c
    portasm_stub.S
)

listenai_include_directories(
    ${LIB_PATH}
)

if (DEFINED CONFIG_ARCS_HAL_WCND)
    listenai_link_libraries(arcs_hal_wcnd)
endif()

# Linker fragment: .sys_init section (multi-level sorted init table)
listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/sys-init-sections.ld
    SLOT ROM SORT_KEY "10-soc"
)

if (NOT DEFINED CONFIG_ARCS_STARTUP_CUSTOM_LINKER_FILE)
    # SoC 可在 soc/<chip>/startup/system.ld 放置专属链接脚本覆盖通用模板
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/system.ld)
        listenai_set_linker_script(${CMAKE_CURRENT_SOURCE_DIR}/system.ld)
    else()
        listenai_set_linker_script(${ARCS_SDK_BASE}/soc/common/system.ld)
    endif()
endif()

if (DEFINED CONFIG_BACK_TRACE)
    add_compile_definitions(CFG_BACK_TRACE)
    # Define the project executable name for backtrace
    if(DEFINED PROJECT_NAME)
        add_compile_definitions(PROJECT_EXECUTABLE_NAME="${PROJECT_NAME}")
    else()
        add_compile_definitions(PROJECT_EXECUTABLE_NAME="firmware")
    endif()
endif()

if (DEFINED CONFIG_CPP_EXCEPTIONS)
    add_compile_options(
        -fexceptions
    )
endif()
