
if (DEFINED CONFIG_SDK_MODULE_LISA_PLAYER)
    if (CONFIG_SDK_MODULE_LISA_PLAYER_SOURCE_CODE)
        message(STATUS "lisa player source code")
        if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lisa_player/.git)
            execute_process(
                COMMAND git clone ${CONFIG_SDK_MODULE_LISA_PLAYER_SOURCE_CODE_URL} lisa_player
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_CLONE_RESULT
            )
            if (NOT GIT_CLONE_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to clone lisa_player repository")
            endif()
        endif()

        execute_process(
            COMMAND git fetch --unshallow
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lisa_player
            ERROR_QUIET
        )
        execute_process(
            COMMAND git fetch origin
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lisa_player
        )
        execute_process(
            COMMAND git checkout ${CONFIG_SDK_MODULE_LISA_PLAYER_SOURCE_CODE_REVERSION}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lisa_player
            RESULT_VARIABLE GIT_CHECKOUT_RESULT
        )
        if (NOT GIT_CHECKOUT_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to checkout lisa_player commit: ${CONFIG_SDK_MODULE_LISA_PLAYER_SOURCE_CODE_REVERSION}")
        endif()

        add_subdirectory(lisa_player)
    else()
        listenai_link_libraries(m)
        if (CONFIG_4G_MODULE)
            listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player_4g.a)
            listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track_4g.a)
        else()
            listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a)
            listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
        endif()
        listenai_include_directories(./)
    endif()
endif()
