listenai_library_sources(display_trans_ctx.c)
listenai_include_directories(.)

if(DEFINED CONFIG_LISA_DISPLAY_INTERFACE_QSPI)
    listenai_library_sources(qspi_trans.c)
elseif(DEFINED CONFIG_LISA_DISPLAY_INTERFACE_SPI_4_LINE)
    listenai_library_sources(spi_4line_trans.c)
endif()