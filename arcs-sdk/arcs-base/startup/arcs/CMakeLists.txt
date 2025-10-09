set(TOP_PATH ${CMAKE_HOME_DIRECTORY})
set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB_RECURSE LIB_SRCS "system.c" "*.[cS]")

enable_language(ASM)

listenai_library_named(startup)

if (DEFINED CONFIG_ARCS_HAL_DEBUG)
    list(FILTER LIB_SRCS EXCLUDE REGEX "sysport.c")
    list(FILTER LIB_SRCS EXCLUDE REGEX "dmacpy.c")
endif()

listenai_library_sources(
    ${LIB_SRCS}
)

listenai_include_directories(
    ${LIB_PATH}
)

if (DEFINED CONFIG_ARCS_HAL_WCND)
    listenai_link_libraries(arcs_hal_wcnd)
endif()

if (NOT DEFINED CONFIG_MEM_CONFIG_USE_CUSTOM_FILE)
    listenai_include_directories(memmap)
endif()

if (NOT DEFINED CONFIG_ARCS_STARTUP_CUSTOM_LINKER_FILE)
    listenai_set_linker_script(${CMAKE_CURRENT_SOURCE_DIR}/system.ld)
endif()

if (DEFINED CONFIG_BACK_TRACE)
    add_compile_definitions(CFG_BACK_TRACE)
    # Define the project executable name for backtrace
    if(DEFINED PROJECT_NAME)
        add_compile_definitions(PROJECT_EXECUTABLE_NAME="${PROJECT_NAME}")
    else()
        add_compile_definitions(PROJECT_EXECUTABLE_NAME="arcs")
    endif()
endif()

if (DEFINED CONFIG_CPP_EXCEPTIONS)
    add_compile_options(
        -fexceptions
    )
endif()
