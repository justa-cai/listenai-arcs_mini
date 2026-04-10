#if CONFIG_MEM_LUNA_SHARED_SIZE > 0
    .luna.text : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.text)
    } >LUNA AT>ROM

    .luna.data : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.data)
    } >LUNA AT>ROM

    .luna.bss(NOLOAD) : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.bss)
    } >LUNA

/* LUNA shared memory must be within 64KB from 0x20040000 */
ASSERT((CONFIG_MEM_LUNA_SHARED_BASE >= 0x20040000),
    "error: LUNA shared memory base must be >= 0x20040000")
ASSERT(((CONFIG_MEM_LUNA_SHARED_BASE + CONFIG_MEM_LUNA_SHARED_SIZE) <= (0x20040000 + 64 * 1024)),
    "error: LUNA shared memory must be within 64KB from 0x20040000")
#endif
