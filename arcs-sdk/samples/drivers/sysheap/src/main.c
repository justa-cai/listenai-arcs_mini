#include "sysheap.h"
#include <stdio.h>
#include <stdint.h>

size_t size = 1024;     // 每块内存大小：1024 字节
size_t align = 4;       //4字节对齐
size_t num = 16;        // 分配块数：16 块
// 从SRAM分配内存
void malloc_from_sram(void)
{
    void *sram_ptr = inram_malloc(align, size);

    if (sram_ptr == NULL) {
        printf("SRAM malloc failed!\n");
        return;
    } else {
        printf("SRAM malloc success: %p\n", sram_ptr);
    }
    inram_free(sram_ptr);
}
// 从PSRAM分配内存
void malloc_from_psram(void)
{
    void *psram_ptr = psram_calloc_align(align, num, size);

    if (psram_ptr == NULL) {
        printf("PSRAM malloc failed!\n");
        return;
    } else {
        printf("PSRAM malloc success: %p\n", psram_ptr);
    }
    psram_free(psram_ptr);
}

int main(int argc, char **argv)
{
    // sysheap在main函数前初始化
    printf("Hello, world! sysheap\n");
    malloc_from_sram();
    malloc_from_psram();
    printf("sysheap end.");
    return 0;
}
