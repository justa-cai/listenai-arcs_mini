if(CONFIG_LISA_JPEG)

listenai_library_named(lisa_jpeg)

listenai_library_sources(
    lisa_jpeg.c
)

listenai_include_directories(
    ./
)

endif() # CONFIG_LISA_JPEG
