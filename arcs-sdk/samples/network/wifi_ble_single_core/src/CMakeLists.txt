listenai_library_named(app)
listenai_library_sources(
    main.c
    app_at_cmd.c
    app_net_cfg.c
    fs/user_fs.c
)

listenai_include_directories(
    ./
    fs/
)
