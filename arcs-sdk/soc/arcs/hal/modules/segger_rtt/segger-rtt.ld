    .segger_rtt (NOLOAD) : ALIGN(4)
    {
        . = ALIGN(4);
        KEEP(*(.segger_rtt .segger_rtt.*))
        . = ALIGN(4);
    } >RAM AT>RAM
