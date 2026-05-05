listenai_library_named(app)

listenai_library_sources(
    main.c
    nettest_result.c
    nettest_wifi.c
    nettest_iperf3.c
    nettest_shell.c
    fs/user_fs.c
)

listenai_include_directories(
    ./
    ./fs
)
