listenai_library_named(app)

listenai_library_sources(
    main.c
    fs/user_fs.c
    net/net_connect.c
)

listenai_include_directories(
    fs
    net
    .
)
