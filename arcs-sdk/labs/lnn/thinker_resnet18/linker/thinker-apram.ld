#if CONFIG_MEM_APRAM_SIZE > 0
    .thinker.apram.noinit (NOLOAD) : ALIGN(64) {
        . = ALIGN(64);
        __thinker_apram_start = .;
        KEEP(*(.thinker.apram.noinit))
        KEEP(*(.thinker.apram.noinit.*))
        . = ALIGN(64);
        __thinker_apram_end = .;
    } >APRAM
#endif
