/*
 * Boot-specific linker sections injected via COMPONENTS slot.
 *
 * Layout in ROM (LMA):
 *   .boot_f.scatab  — boot_f 阶段散装加载表（引用下面各段）
 *   .boot.text      — boot 专属 ROM 代码（不随 scatter 搬运，直接运行）
 *   .boot_f.ramcode — 早期初始化代码，LMA=ROM → VMA=SRAM
 *   .boot_f.data    — 早期初始化数据，LMA=ROM → VMA=SRAM
 *   .boot_f.bss     — 早期初始化 BSS，VMA=SRAM（清零，不从 ROM 拷贝）
 */

    .boot_f.scatab : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(__boot_f_scat_copy_base = .);
            SCATLOAD(.boot_f.ramcode)
            SCATLOAD(.boot_f.data)
        PROVIDE(__boot_f_scat_copy_last = .);

        . = ALIGN(4);
        PROVIDE(__boot_f_scat_zero_base = .);
            SCATZERO(.boot_f.bss)
        PROVIDE(__boot_f_scat_zero_last = .);
        . = ALIGN(4);
    } >ROM AT>ROM

    .boot.text : ALIGN(4) {
        . = ALIGN(4);
        * (.boot.text)
        * (.boot.text.*)
        /* LCD 提示图等 const 资源：放 flash XIP，不占 SRAM。*/
        * (.boot.image)
        * (.boot.image.*)
        . = ALIGN(4);
    } >ROM AT>ROM

    .boot_f.ramcode : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_boot_f_ramcode_start = .);
        KEEP(* (.boot_f.ramcode))
        KEEP(* (.boot_f.ramcode.*))
        *libc.a:libc_a-rand.o(.text .text.* .stext .stext.*)
        *libc_nano.a:libc_a-rand.o(.text .text.* .stext .stext.*)
        * (.ramcode)

        /* Early init HAL sources compiled directly (not via libarcs-hal.a) */
        *clock_config.c*(.text .text.*)
        *ClockManager.c*(.text .text.*)
        *PSRAMManager.c*(.text .text.*)
        . = ALIGN(4);
        PROVIDE(_boot_f_ramcode_end = .);
    } >SRAM AT>ROM

    .boot_f.data : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_boot_f_data_start = .);
        KEEP(* (.boot_f.data))
        KEEP(* (.boot_f.data.*))
        *libc.a:libc_a-impure.o(.data .data.* .sdata .sdata.*)
        *libc_nano.a:libc_a-impure.o(.data .data.* .sdata .sdata.*)
        * (.data)
        * (.sdata .sdata.*)
        * (.rodata .rodata.*)
        * (.srodata .srodata.*)

        /* Early init HAL sources: rodata (const tables used by BootClock_Init etc.) */
        *clock_config.c*(.rodata .rodata.* .srodata .srodata.*)
        *ClockManager.c*(.rodata .rodata.* .srodata .srodata.*)
        *PSRAMManager.c*(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4);
        PROVIDE(_boot_f_data_end = .);
    } >SRAM AT>ROM

    .boot_f.bss : ALIGN(4) {
        . = ALIGN(4);
        PROVIDE(_boot_f_bss_start = .);
        KEEP(* (.boot_f.bss))
        KEEP(* (.boot_f.bss.*))
        *libc.a:libc_a-impure.o(.bss .bss.* .sbss .sbss.* COMMON)
        *libc_nano.a:libc_a-impure.o(.bss .bss.* .sbss .sbss.* COMMON)
        * (.bss)
        * (.sbss .sbss.*)
        . = ALIGN(4);
        PROVIDE(_boot_f_bss_end = .);
    } >SRAM AT>SRAM
