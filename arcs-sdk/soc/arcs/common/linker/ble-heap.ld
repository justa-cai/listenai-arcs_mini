#if CONFIG_BLE
    PROVIDE(__BT_HEAP_SIZE = 24K);

    /* BT Heap in PSRAM */
    .bt_heap (NOLOAD): ALIGN(16) {
        . = ALIGN(16);
        PROVIDE( __ext_ram_start = . );
        . = . + __BT_HEAP_SIZE;
        PROVIDE( __ext_ram_end = . );
    } >PSRAM
#endif
