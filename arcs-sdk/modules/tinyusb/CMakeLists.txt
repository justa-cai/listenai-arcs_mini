if (DEFINED CONFIG_TINY_USB)

    set(TINYUSB_TARGET "psram_tinyusb")
    set(TINYUSB_CONFIG_TARGET "${TINYUSB_TARGET}_config")

    add_library(${TINYUSB_CONFIG_TARGET} INTERFACE)

    if (DEFINED CONFIG_TINY_USB_CONFIG_FILE)
        target_compile_definitions(
            ${TINYUSB_CONFIG_TARGET}
            INTERFACE
            CFG_TUSB_CONFIG_FILE=CONFIG_TINY_USB_CONFIG_FILE
        )
    endif()

    target_compile_definitions(
        ${TINYUSB_CONFIG_TARGET}
        INTERFACE
        CFG_TUSB_DEBUG_PRINTF=printk
    )

    target_include_directories(${TINYUSB_CONFIG_TARGET} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_subdirectory(src)

    target_sources(${TINYUSB_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/portable/listenai/ls566x/dcd_ls566x.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/portable/mentor/musb/dcd_musb.c
    )
    target_include_directories(${TINYUSB_TARGET} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/portable/listenai/ls566x
        ${CMAKE_CURRENT_SOURCE_DIR}/src/portable/mentor/musb
    )

    target_compile_options(${TINYUSB_TARGET} PRIVATE -O3)
    target_compile_options(${TINYUSB_TARGET} PRIVATE -w)

    listenai_append_cmake_library(${TINYUSB_TARGET})
    target_link_libraries(${TINYUSB_TARGET} PUBLIC listenai_interface)

    listenai_link_libraries(${TINYUSB_TARGET})

endif()
