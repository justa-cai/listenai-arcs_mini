if (DEFINED CONFIG_SDK_MODULE_RESAMPLE)
    listenai_library_named(resample)
    listenai_compile_definitions(FIXED_POINT)
    listenai_compile_definitions(EXPORT=)
    listenai_compile_definitions(RISCV)
    listenai_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/speex/resample.c
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/pcm_resample.c
    )

    listenai_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/libs
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/speex
    )

    listenai_link_libraries(freertos)
endif()
