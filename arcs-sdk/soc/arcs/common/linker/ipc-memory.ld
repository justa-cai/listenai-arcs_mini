/* IPC shared RAM region (only when isolated from SRAM) */
#if CONFIG_MEM_IPC_ISOLATED
    IPC_RAM(rwx) : ORIGIN = CONFIG_MEM_IPC_BASE, LENGTH = CONFIG_MEM_IPC_SIZE
#endif
