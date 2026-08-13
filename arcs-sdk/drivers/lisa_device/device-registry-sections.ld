    /* LISA Device 设备注册段 */
    .lisa_device_registry : {
        . = ALIGN(4);
        __lisa_device_registry_start = .;
        KEEP (*(.lisa_device_registry))
        KEEP (*(SORT(.lisa_device_registry.*)))
        . = ALIGN(4);
        __lisa_device_registry_end = .;
    } >ROM AT>ROM

    /* LISA Device 可选 PM 能力注册段 */
    .lisa_device_pm_registry : {
        . = ALIGN(4);
        __lisa_device_pm_registry_start = .;
        KEEP (*(.lisa_device_pm_registry))
        KEEP (*(SORT(.lisa_device_pm_registry.*)))
        . = ALIGN(4);
        __lisa_device_pm_registry_end = .;
    } >ROM AT>ROM
