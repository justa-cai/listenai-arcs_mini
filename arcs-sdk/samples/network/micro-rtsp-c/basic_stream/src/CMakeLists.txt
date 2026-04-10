listenai_library_named(app)
listenai_library_sources(
    main.c
     fs/user_fs.c
)

listenai_include_directories(
    ./
    fs/
)

target_link_libraries(app PRIVATE m)