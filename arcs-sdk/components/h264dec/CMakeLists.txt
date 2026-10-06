if(CONFIG_H264DEC)

listenai_library_named(h264dec)

listenai_library_sources(
    h264dec.c
)

listenai_include_directories(
    ./
)

# 解码热路径用 -O2（SDK 全局 -Os 体积优先，CAVLC/IDCT/运动补偿在 -O2 下
# 快 20~40%，44KB text 的代价完全可接受）
listenai_library_compile_options(PRIVATE -O2)

endif() # CONFIG_H264DEC
