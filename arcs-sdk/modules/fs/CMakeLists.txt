if (CONFIG_FILE_SYSTEM)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/subfs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lsfs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lvfs)
endif()

if (CONFIG_DISK_DRIVER)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/disk)
endif()

if (DEFINED CONFIG_FILE_SYSTEM OR DEFINED CONFIG_DISK_DRIVER)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/port)
endif()
