if (DEFINED CONFIG_ARCS_HAL_MODULE_SHELL)
    listenai_library_named(arcs_hal_module_shell)

    file(GLOB SHELL_SRCS "*.c")

    # Add source files
    listenai_library_sources(
        ${SHELL_SRCS}
    )

    # Add include directories
    listenai_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}  # For generated sw_version.h
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARCS_HAL_LIB_PATH}/include/debug
        ${ARCS_HAL_LIB_PATH}/include/driver
        ${ARCS_HAL_LIB_PATH}/chip/arcs/include
        ${ARCS_HAL_LIB_PATH}/include/bsp
        ${ARCS_HAL_LIB_PATH}/include/nvs
        ${ARCS_HAL_LIB_PATH}/include/CMSIS
        ${ARCS_HAL_LIB_PATH}/include/CMSIS/core
        ${ARCS_HAL_LIB_PATH}/modules/driver/private_include
    )

    listenai_library_compile_options_ifdef(
        CONFIG_ARCS_HAL_WARNING_DISABLE
        PRIVATE
        -w
    )

endif()
