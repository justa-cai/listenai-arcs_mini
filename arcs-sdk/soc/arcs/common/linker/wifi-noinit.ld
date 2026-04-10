    /* wifi 校准数据预留大小 */
    PROVIDE(_lcali = CONFIG_MEM_WIFI_CALIBRATION_SIZE);
    /* wifi 跟踪数据预留大小 */
    PROVIDE(_trace_len = CONFIG_MEM_WIFI_TRACE_SIZE);

    .wifi.hw_reserved (NOLOAD):
    {
        /* WiFi 硬件保留区域，不可分配 */
        . += 0x80;
    } > WIFI_RAM

    .wifi.noinit (NOLOAD):
    {
        . = ALIGN(4);
        _wifi_ram_start = . ;
        *(WIFI_CALI)
#if CONFIG_WIFI_CALI_BUF
        _scali = .;
        . += _lcali;
        _ecali = .;
#endif
        _trace_start = .;
        . += _trace_len;
        _trace_end = .;
        _sshram = . ;
        *(SHAREDRAMIPC)
        *(IPC_WIFI_SHARE)
        *(SHAREDRAM)
        _eshram = . ;
        . = ALIGN(4);

        _wifi_ram_end = . ;
    } > WIFI_RAM

/* WIFI外设只能访问 0x20000000 - 0x20000000 + 256 * 1024这段内存  */
ASSERT  ((_wifi_ram_start >= (0x20000000)), "error: wifi ram start must be greater than 0x20000000")
ASSERT  ((_wifi_ram_end <= (0x20000000 + 256 * 1024)), "error: wifi ram end must be less than 0x20000000 + 256 * 1024")
