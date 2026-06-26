if(CONFIG_MIDDLEWARE_SD_MUSIC)

listenai_library_named(sd_music)

listenai_library_sources(
    sd_music_card.c
    sd_music_scanner.c
    sd_music_http.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

endif()
