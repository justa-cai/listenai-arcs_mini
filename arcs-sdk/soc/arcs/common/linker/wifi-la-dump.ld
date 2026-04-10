#if CONFIG_WIFI
    .wifi_la_dump (NOLOAD) : {
        _slaram = .;
        . += CONFIG_MEM_WIFI_LA_DUMP_SIZE;
        _elaram = .;
    } > PSRAM
#endif
