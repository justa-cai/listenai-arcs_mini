#include "esp_heap_caps.h"

HEAP_IRAM_ATTR void esp_heap_adjust_alignment_to_hw(size_t *p_align, size_t *p_size, uint32_t *p_caps)
{
    if (*p_caps & (MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED)) {
        // MALLOC_CAP_CACHE_ALIGNED is not a real flag the heap_base component will 
        // understand; it only sets alignment (which we handled here)
        *p_caps &= ~MALLOC_CAP_CACHE_ALIGNED;
        *p_align = 32;
        *p_size = (*p_size + 31) / 32 * 32;
    }
}

HEAP_IRAM_ATTR bool esp_ptr_in_diram_iram(const void *p)
{
    return false;
}

HEAP_IRAM_ATTR bool esp_ptr_in_diram_dram(const void *p)
{
    return false;
}

HEAP_IRAM_ATTR void *esp_ptr_diram_dram_to_iram(const void *p)
{
    return (void *)p;
}

HEAP_IRAM_ATTR bool esp_dram_match_iram(void)
{
    return true;
}
