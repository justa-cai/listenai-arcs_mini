if(DEFINED CONFIG_SDK_MODULE_COREMQTT_AGENT)

listenai_library_named(coreMQTT-Agent)

listenai_library_sources(
    source/core_mqtt_agent_command_functions.c
    source/core_mqtt_agent.c)

listenai_include_directories(
    source/include
)

endif()