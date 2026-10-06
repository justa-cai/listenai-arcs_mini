if(CONFIG_H264DEC)

listenai_library_named(h264dec)

listenai_library_sources(
    h264dec.c
)

listenai_include_directories(
    ./
)

endif() # CONFIG_H264DEC
