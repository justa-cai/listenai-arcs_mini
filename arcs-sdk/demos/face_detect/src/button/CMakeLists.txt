
if(CONFIG_BUTTON)

listenai_library_named(psram_button)

listenai_library_sources(
    button.c
    button_adc.c
    button_gpio.c
    FlexibleButton/flexible_button.c
    # test/test_gpio_button.c
    # test/test_adc_button.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/FlexibleButton
)

endif()
