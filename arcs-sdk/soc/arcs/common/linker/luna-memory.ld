/* LUNA private memory region (dedicated to LUNA core) */
    LUNA_PRIV_RAM(rwx) : ORIGIN = CONFIG_MEM_LUNA_BASE, LENGTH = CONFIG_MEM_LUNA_SIZE

/* LUNA shared memory region (must be within 64KB from 0x20040000) */
#if CONFIG_MEM_LUNA_SHARED_SIZE > 0
    LUNA_RAM(rwxa) : ORIGIN = CONFIG_MEM_LUNA_SHARED_BASE, LENGTH = CONFIG_MEM_LUNA_SHARED_SIZE
#endif
