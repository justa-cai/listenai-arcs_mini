if(DEFINED CONFIG_SDK_MODULE_COREMQTT)
listenai_library_named(coreMQTT)

add_definitions(-DMQTT_DO_NOT_USE_CUSTOM_CONFIG)

listenai_library_sources(
    source/core_mqtt_serializer.c
    source/core_mqtt_state.c
    source/core_mqtt.c
)

listenai_include_directories(
    source/include
    source/interface
)
endif()