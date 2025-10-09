if(DEFINED CONFIG_COMPONENT_CAMERA)
    SET(SDK_MODULES_CAMERA_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    listenai_library_named(module_camera)

    listenai_library_sources(camera_core.c camera_xfer.c)

    add_subdirectory(sensors)

    if(DEFINED CONFIG_CAMERA_INTERFACE_SPI)
        add_subdirectory(spi_camera)
    elseif(DEFINED CONFIG_CAMERA_INTERFACE_DVP)
        add_subdirectory(dvp_camera)
    endif()

    listenai_include_directories(${SDK_MODULES_CAMERA_PATH})
endif()
