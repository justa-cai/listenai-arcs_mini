    .luna.text : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.text)
        *(.sharedmem.text.*)
        . = ALIGN(8);
    } >SRAM AT>ROM

    .luna.data : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.data)
        *(.sharedmem.data.*)
        . = ALIGN(8);
    } >SRAM AT>ROM

    .luna.bss(NOLOAD) : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.bss)
        *(.sharedmem.bss.*)
        . = ALIGN(8);
    } >SRAM

    PROVIDE(__luna_shared_start = ADDR(.luna.text));
    PROVIDE(__luna_shared_end = ADDR(.luna.bss) + SIZEOF(.luna.bss));
