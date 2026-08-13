/*
 * VenusA LUNA 共享段布局
 *
 * 只拦截输入文件中的 .sharedmem.text/.data/.bss 基础段，避免通配
 * .sharedmem.* 误收集应用自定义子段。带初始化内容的 text/data 使用
 * >LUNA AT>ROM，启动 scatter 会在进入 main() 前拷贝到 LUNA 共享 SRAM；
 * bss 使用 NOLOAD，并由 scatter 清零。
 */
#if CONFIG_MEM_LUNA_SHARED_SIZE > 0
    .luna.text : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.text)
        . = ALIGN(8);
    } >LUNA AT>ROM

    .luna.data : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.data)
        . = ALIGN(8);
    } >LUNA AT>ROM

    .luna.bss (NOLOAD) : ALIGN(8) {
        . = ALIGN(8);
        *(.sharedmem.bss)
        . = ALIGN(8);
    } >LUNA

    PROVIDE(__luna_shared_start = ADDR(.luna.text));
    PROVIDE(__luna_shared_end = ADDR(.luna.bss) + SIZEOF(.luna.bss));

    ASSERT(__luna_shared_end <= (CONFIG_MEM_LUNA_SHARED_BASE + CONFIG_MEM_LUNA_SHARED_SIZE),
        "error: VenusA LUNA shared sections overflow LUNA_RAM")
#endif
