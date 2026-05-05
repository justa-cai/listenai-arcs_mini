# set(PLAYER_SRC ON)

if (DEFINED CONFIG_SDK_MODULE_LISA_PLAYER)
    if (NOT DEFINED PLAYER_SRC)
        listenai_link_libraries(m)
        listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a)
        listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
    else()
        # 如果定义了 PLAYER_SRC，则编译源码
        add_subdirectory(lisa_player)
    endif()
    listenai_include_directories(./)
endif()
