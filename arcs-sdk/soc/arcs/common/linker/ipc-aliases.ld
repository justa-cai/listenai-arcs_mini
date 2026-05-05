/* 如果不隔离 ipc shared ram 的话, 从sram中分配 */
#if !defined(CONFIG_MEM_IPC_ISOLATED) || !CONFIG_MEM_IPC_ISOLATED
REGION_ALIAS("IPC_RAM", SRAM)
#endif
