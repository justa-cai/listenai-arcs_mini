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
listenai_library_sources_ifdef(CONFIG_SOC_EARLY_LOG ${ARCS_SDK_BASE}/soc/common/early_log.c)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (DEFINED CONFIG_ARCS_HAL_WCND)
    listenai_link_libraries(arcs_hal_wcnd)
endif()

if (NOT DEFINED CONFIG_ARCS_STARTUP_CUSTOM_LINKER_FILE)
    # SoC 可在 soc/<chip>/startup/system.ld 放置专属链接脚本覆盖通用模板
    set(STARTUP_LINKER_SCRIPT ${ARCS_SDK_BASE}/soc/common/system.ld)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/system.ld)
        set(STARTUP_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/system.ld)
    endif()
    listenai_set_linker_script(${STARTUP_LINKER_SCRIPT})
endif()

if (DEFINED CONFIG_BACK_TRACE)
    add_compile_definitions(CFG_BACK_TRACE)
    # Define the project executable name for backtrace
    if(DEFINED PROJECT_NAME)
        set(PROJECT_EXECUTABLE_NAME_VALUE "${PROJECT_NAME}")
    else()
        set(PROJECT_EXECUTABLE_NAME_VALUE "firmware")
    endif()
    add_compile_definitions(PROJECT_EXECUTABLE_NAME="${PROJECT_EXECUTABLE_NAME_VALUE}")
endif()

if (DEFINED CONFIG_CPP_EXCEPTIONS)
    add_compile_options(
        -fexceptions
    )
endif()
