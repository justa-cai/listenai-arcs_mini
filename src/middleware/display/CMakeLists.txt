listenai_library_named(app_dis)

listenai_library_sources(
    app_display.c
    lv_fs_net.c
    lv_img_net_loader.c
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)
