/* PM startup 保留 .bss 段。
 *
 * 这里收集所有用 _PM_STARTUP_BSS 注解的全局 / 静态变量
 * (sleep_save → wake → snapshot_restore 之间被读写的关键状态：
 *  pm_snapshot_ctx / pm_env / clock_cfg_reg_info / psram_cfg_reg_info /
 *  vrtc_flags / __light_sleep_stash 等)。
 *
 * 必须落在 SRAM 前 320KB 的 retention 覆盖区内，唤醒时 snapshot_restore
 * 还没跑就要被读取。__pm_startup_reserved_start/end 给 pm_snapshot_add_region
 * 校验时排除该区。 */
    .pm_startup_bss (NOLOAD) : ALIGN(16)
    {
        __pm_startup_reserved_start = .;
        *(.pm.startup_bss .pm.startup_bss.*)
        __pm_startup_reserved_end = .;
    } >RAM AT>RAM
