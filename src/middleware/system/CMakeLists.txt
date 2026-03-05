listenai_library_named(system)

listenai_library_sources(
    c_datetime.c
    listen_system.c
    user_sntp.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
