listenai_library_named(aiui_alarm)

listenai_library_sources(
    alarm.c
    alarm_ring.c
    alarm_nvs.c
    alarm_next.c
    alarm_constructor.c
    alarm_time_utils.c
    alarm_api.c
    alarm_handler.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(
    ${LISTENAI_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)
