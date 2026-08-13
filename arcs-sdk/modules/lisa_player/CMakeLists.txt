# set(PLAYER_SRC ON)
option(LISA_PLAYER_DECODE_LOG_AUDIO_CACHE
    "Enable lisa_player decode audio cache logging"
    OFF)

if (DEFINED CONFIG_SDK_MODULE_LISA_PLAYER)
    if (NOT DEFINED PLAYER_SRC)
        listenai_link_libraries(m)
        # The prebuilt HTTP receiver gives up after six 500 ms socket timeouts.
        # Keep the vendor archive unchanged and make only _recv overridable so
        # app_player can tolerate delayed stream_tts data.
        set(LISA_PLAYER_PATCHED_LIBRARY
            "${CMAKE_CURRENT_BINARY_DIR}/liblisa_player_http_override.a")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a"
            "${LISA_PLAYER_PATCHED_LIBRARY}"
            COPYONLY)
        execute_process(
            COMMAND "${CMAKE_OBJCOPY}" --weaken-symbol=_recv
                    "${LISA_PLAYER_PATCHED_LIBRARY}"
            RESULT_VARIABLE LISA_PLAYER_OBJCOPY_RESULT)
        if (NOT LISA_PLAYER_OBJCOPY_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Failed to make lisa_player _recv overridable: ${LISA_PLAYER_OBJCOPY_RESULT}")
        endif()

        listenai_link_libraries(${LISA_PLAYER_PATCHED_LIBRARY})
        listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a)
    else()
        # 如果定义了 PLAYER_SRC，则编译源码，并在构建完成后更新当前目录的预编译库
        add_subdirectory(lisa_player)
        if (CONFIG_SDK_MODULE_LISA_PLAYER_DECODE_LOG_AUDIO_CACHE OR LISA_PLAYER_DECODE_LOG_AUDIO_CACHE)
            target_compile_definitions(lisa_player PRIVATE LISA_PLAYER_DECODE_LOG_AUDIO_CACHE)
        endif()
        add_custom_target(lisa_player_copy_libraries ALL
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:lisa_player>"
                "${CMAKE_CURRENT_SOURCE_DIR}/liblisa_player.a"
            COMMAND ${CMAKE_COMMAND} -E copy
                "$<TARGET_FILE:platform_track>"
                "${CMAKE_CURRENT_SOURCE_DIR}/libplatform_track.a"
            COMMENT "Copy lisa_player libraries back to ${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM
        )
        add_dependencies(lisa_player_copy_libraries lisa_player platform_track)
    endif()
    listenai_include_directories(./)
endif()
