listenai_library_named(app)

listenai_library_sources(
    middleware/user_fs.c
    main.c
)

listenai_include_directories(
    ./
    ./middleware
)
