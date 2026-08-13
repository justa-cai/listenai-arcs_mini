/*
 * 仅保留符号保障 HAL 库 PM 代码编译通过。
 *
 * HAL 不可改，pm_impl.c 仍 extern 下列符号；本文件用零字节占位段定义它们，
 * 让链接通过、原配拷贝循环被 if 守卫拦成 noop。
 *
 *   _mem_copy_func_start / _end / _lma
 *     原用于把 mem_copy() 从 flash 自举拷到运行位置。现 mem_copy 已搬到
 *     .fast.text 由 scatload 装载，自举不再需要。
 *
 *   __copy_table_start__ / __copy_table_end__
 *     原用于 wake 时遍历 (LMA, VMA, end) 三元组、把段从 flash 重装回 RAM。
 *     现 copy_table 已清空，wake 重装规划由 PSRAM snapshot 接管。
 */
    .text.mem_copy : ALIGN(4)
    {
        . = ALIGN(4);
        _mem_copy_func_start = .;
        _mem_copy_func_end = .;
        . = ALIGN(4);
    } >ROM AT>ROM
    PROVIDE(_mem_copy_func_lma   = LOADADDR(.text.mem_copy));

    .copy_table : ALIGN(4)                                                                                                                                                                                                                                                         
    {                                                                                                                                                                                                                                                                       
        . = ALIGN(4);                                                                                                                                                                                                                                                            
        __copy_table_start__ = .;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       
        __copy_table_end__ = .;                                                                                                                                                                                                                                                    
        . = ALIGN(4);                                                                                                                          
    } >ROM AT>ROM  
