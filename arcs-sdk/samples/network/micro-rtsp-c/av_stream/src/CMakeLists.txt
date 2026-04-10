listenai_library_named(app)
listenai_library_sources(
    main.c
    audio.c
    camera.c
    # jpeg_encoder_example.c
    jpeg_encoder_ext.c
     fs/user_fs.c
)

listenai_include_directories(
    ./
    fs/
)

target_link_libraries(app PRIVATE m)