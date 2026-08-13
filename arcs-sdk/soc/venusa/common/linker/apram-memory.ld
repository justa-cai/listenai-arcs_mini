/*
* 算法专用 SRAM 区域（默认给 FD/LSFace 等算法工作区）。
* 这里只声明地址窗口供链接器识别，并不放置任何输出 section；
* 与其它区域不重叠由 mem-layout-check.ld 的 ASSERT 和各 region
* 的 LENGTH 边界共同保证，本文件不单独起保留作用。
*/
#if defined(CONFIG_MEM_APRAM_SIZE) && CONFIG_MEM_APRAM_SIZE > 0
    APRAM(rwx) : ORIGIN = CONFIG_MEM_APRAM_BASE, LENGTH = CONFIG_MEM_APRAM_SIZE
#endif
