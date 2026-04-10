/*
 * 应用注册表段
 *
 * 与 SDK 的 .lisa_device_registry / .sys_init 段原理相同：
 *   - KEEP() 防止未直接引用的注册条目被回收
 *   - SORT() 确保条目按段名字典序排列
 *   - 起止符号用于运行时遍历
 *
 * 只读段，直接从 ROM 执行，无需散加载。
 */
    .app_registry : ALIGN(4) {
        . = ALIGN(4);
        __app_registry_start = .;
        KEEP(*(SORT(.app_registry.*)))
        . = ALIGN(4);
        __app_registry_end = .;
    } >ROM AT>ROM
