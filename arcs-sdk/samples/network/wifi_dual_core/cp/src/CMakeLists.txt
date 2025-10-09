listenai_library_named(app)
listenai_library_sources(
    middleware/user_wifi.c
    middleware/user_fs.c
    middleware/sdmmc_init.c
    main.c
    get_tests.cpp
    tls_certificate.c
    arcs_flash_if.c
)

listenai_include_directories(
    ./
    ./middleware
    ./middleware/rec
    ./middleware/play
    ./user_config
    ./algo
    ./algo/xttsservice
    ./algo/wordseg/include/
    ./algo/wordseg/source
)


add_subdirectory(low_port)
