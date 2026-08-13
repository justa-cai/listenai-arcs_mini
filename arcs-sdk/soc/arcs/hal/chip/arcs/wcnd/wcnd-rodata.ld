    .cali_rodata : ALIGN(4)
    {
        . = ALIGN(4);
        KEEP(*(.cali_rodata .cali_rodata.*))
        . = ALIGN(4);
    } >ROM AT>ROM
