listenai_library_named(adc_key)

listenai_library_sources(
    adc_key.c
    adc_sample.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
