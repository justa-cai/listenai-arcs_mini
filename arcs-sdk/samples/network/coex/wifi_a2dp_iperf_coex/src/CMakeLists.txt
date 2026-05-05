listenai_library_named(app)

listenai_library_sources(
    main.c
    coex_result.c
    coex_iperf.c
    coex_shell.c
    coex_wifi.c
    coex_bt.c
    coex_bt_audio.c
    fs/user_fs.c
)

listenai_include_directories(
    ./
    ./fs
)
