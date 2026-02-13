listenai_library_named(aiui_alarm)

listenai_library_sources(
    alarm_aiui.c
    alarm.c
    alarm_store.c
    alarm_next.c
    alarm_ring.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(
    ${LISTENAI_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)
