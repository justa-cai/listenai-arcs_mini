listenai_library_named(assistant-controller)

listenai_library_sources(
    assistant_controller.c
    ./view/assistant_view.c
    ./view/view_setting.c
    ./view/view_runner.c
    ./view/view_events.c
    ./view/task_statistics.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/view
)
