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
#endif
