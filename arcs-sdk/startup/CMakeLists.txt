include(${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
add_subdirectory(${CHIP})

if (DEFINED CONFIG_BOOT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/boot.cmake)
endif()
