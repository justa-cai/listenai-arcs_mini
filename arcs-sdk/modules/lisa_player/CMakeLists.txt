
if (DEFINED CONFIG_SDK_MODULE_LISA_PLAYER)
    listenai_link_libraries(m)
    listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a)
    listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
    listenai_include_directories(./)
endif()
