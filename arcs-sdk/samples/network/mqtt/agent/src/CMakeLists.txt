listenai_library_named(app)

add_definitions(-DMQTT_AGENT_DO_NOT_USE_CUSTOM_CONFIG)

listenai_library_sources(
    main.c
    fs/user_fs.c
)

listenai_include_directories(
    ./
    ./fs
)
