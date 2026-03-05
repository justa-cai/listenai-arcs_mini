if (CONFIG_ACOMP)

set(TARGET_NAME psram_acomp-master)
listenai_library_named(${TARGET_NAME})

listenai_library_sources(acomp.c)
listenai_library_sources(acomp_sample.c)

add_subdirectory(ipc)
add_subdirectory(comm)
add_subdirectory(utils)
listenai_add_subdirectory_ifdef(CONFIG_ACOMP_WSP wsp)
listenai_add_subdirectory_ifdef(CONFIG_ACOMP_WAKEUP wakeup)
listenai_add_subdirectory_ifdef(CONFIG_ACOMP_LOGGER logger)
listenai_add_subdirectory_ifdef(CONFIG_ACOMP_FD fd)
listenai_include_directories(./)
target_include_directories(${TARGET_NAME} PUBLIC .)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inlucde)



endif()
