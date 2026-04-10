/*
 * 自定义段定义：.app_config
 *
 * 通过 listenai_add_linker_section(SLOT ROM) 注入到链接脚本。
 * 配合 listenai_add_linker_scatter(SCATLOAD .app_config)，
 * 启动代码会在 main() 之前将此段从 ROM 加载地址复制到 RAM 运行地址。
 *
 * 关键要素：
 *   - ALIGN(4): 保证 4 字节对齐（ARM/RISC-V 要求）
 *   - KEEP():   防止链接器垃圾回收
 *   - >RAM AT>ROM: 运行地址在 RAM，加载地址在 ROM
 *   - 起止符号: __app_config_start / __app_config_end 供 C 代码遍历
 */
    .app_config : ALIGN(4) {
        . = ALIGN(4);
        __app_config_start = .;
        KEEP(*(.app_config))
        KEEP(*(.app_config.*))
        . = ALIGN(4);
        __app_config_end = .;
    } >RAM AT>ROM
