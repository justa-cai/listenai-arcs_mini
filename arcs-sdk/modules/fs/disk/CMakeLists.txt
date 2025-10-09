add_library(disk STATIC "")

target_include_directories(disk PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(disk PRIVATE disk_access.c)

if(CONFIG_DISK_DRIVER_FLASH)
target_sources(disk PRIVATE flashdisk.c)
endif()

if (CONFIG_DISK_DRIVER_RAM)
target_sources(disk PRIVATE ramdisk.c)
endif()

if (CONFIG_DISK_DRIVER_SDMMC)
target_sources(disk PRIVATE sdmmc.c)
endif()

if (CONFIG_DISK_IOCACHE_ENABLE)
target_sources(disk PRIVATE iocache.c)
endif()

target_link_libraries(disk PUBLIC arcs-hal fs_env)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(disk)
endif()