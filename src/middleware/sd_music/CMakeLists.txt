# FatFs 配置覆盖文件 (sd_music_fatfs_config.h) 由 CONFIG_FATFS_FFCONF_NAME 按
# 名字引用, 与本中间件是否编译无关; 因此包含路径始终注册, 否则关闭
# MIDDLEWARE_SD_MUSIC 后 FatFs 会找不到该头文件。
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CONFIG_MIDDLEWARE_SD_MUSIC)

listenai_library_named(sd_music)

listenai_library_sources(
    sd_music_card.c
    sd_music_scanner.c
    sd_music_http.c
)

endif()
