listenai_library_named(audio)

listenai_library_sources(
    app_player.c
    audio_play.c
    pa_manager.c
)

if(NOT DEFINED CONFIG_SDK_MODULE_LISA_PLAYER)
    listenai_link_libraries(m)
    listenai_link_libraries($ENV{ARCS_BASE}/components/app_player/player/liblisa_player.a)
    listenai_link_libraries($ENV{ARCS_BASE}/components/app_player/player/libplatform_track.a)
endif()

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    $ENV{ARCS_BASE}/components/app_player/player
)
