#if CONFIG_MEM_IPC_ISOLATED
    /* ipc shared RAM */
    .ipc.shared (NOLOAD):
    {
        _ipcshram = . ;
        *(SHAREDRAM_AMP_IPC_ENV)
        *(SHAREDRAM_AMP_IPC)
        KEEP(*(SORT(.ipc.*)))
        _eipcshram = . ;
        *(.ic_lock_shared_mem)
    } > IPC_RAM

#if CONFIG_ARCS_HAL_IPC_PRINT
    .ipc.print (NOLOAD):
    {
        . = ALIGN(4);
        _ipc_dbg_start = .;
        . += CONFIG_ARCS_HAL_IPC_PRINT_BUF_SIZE;
        _ipc_dbg_end = .;
    } > IPC_RAM
#endif
#endif
