if (CONFIG_ACOMP)

set(TARGET_NAME psram_acomp-master)
listenai_library_named(${TARGET_NAME})

listenai_library_sources(acomp.c)
listenai_library_sources(acomp_sample.c)

add_subdirectory(ipc)
add_subdirectory(utils)
listenai_add_subdirectory_ifdef(CONFIG_ACOMP_WSP wsp)
target_include_directories(${TARGET_NAME} PUBLIC .)



endif()
