#define LOG_TAG "sys_heap"
#include <lisa_log.h>
#include "memap.h"
#include <string.h>
#include "cache.h"

#include "sysutils.h"

#if CONFIG_MODULE_HEAP
#include "esp_heap_caps_init.h"
#include "esp_heap_caps.h"
#include "heap_memory_layout.h"
#include "heap_private.h"

#ifndef __cntof
#define __cntof(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define MEM_BASE_CAPS (MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_EXEC)
const soc_memory_type_desc_t soc_memory_types[] = {
    {
        .name = "SRAM",
        .caps = {
            /*H*/ [0] = MEM_BASE_CAPS | MALLOC_CAP_INTERNAL,
            /*M*/[1] = 0,
            /*L*/[2] = MEM_BASE_CAPS | MALLOC_CAP_DEFAULT,
        },
    },
};

#if CONFIG_HEAP
static uint8_t default_heap[CONFIG_HEAP_SIZE] __noinit__ = {0};
#endif

#if CONFIG_PSRAM_HEAP
static uint8_t psram_default_heap[CONFIG_PSRAM_HEAP_SIZE] __psram_noinit__ = {0};
#endif


#if CONFIG_PSRAM_NOCACHE_HEAP
static uint8_t psram_nocache_heap[CONFIG_PSRAM_NOCACHE_HEAP_SIZE] __attribute__((section(".psram.nocache_heap"))) ;
#if ((CONFIG_PSRAM_NOCACHE_HEAP_SIZE & (CONFIG_PSRAM_NOCACHE_HEAP_SIZE - 1)) != 0)
#error "CONFIG_PSRAM_NOCACHE_HEAP_SIZE must be a power of 2"
#endif

#endif

const size_t soc_memory_type_count = __cntof(soc_memory_types);
const soc_memory_region_t soc_memory_regions[] = {
#if CONFIG_HEAP
    {.start = (intptr_t)default_heap, .size = CONFIG_HEAP_SIZE, .type = 0, .iram_address = 0, .startup_stack = 0},
#endif
};
const size_t soc_memory_region_count = __cntof(soc_memory_regions);

size_t soc_get_available_memory_region_max_count(void)
{
    return soc_memory_region_count;
}

size_t soc_get_available_memory_regions(soc_memory_region_t *regions)
{
    memcpy(regions, soc_memory_regions, sizeof(soc_memory_regions));
    return soc_memory_region_count;
}

static void heap_travel_cb(void *start, void *end, multi_heap_info_t *info)
{
    LISA_LOGI(LOG_TAG, "%p %p %12d %12d %12d %12d %12d %12d %12d\n", start, end, info->allocated_blocks, info->free_blocks,
          info->total_blocks, info->largest_free_block, info->total_allocated_bytes, info->total_free_bytes,
          info->minimum_free_bytes);
}

void heap_summary_info(void)
{
    void heap_caps_travel(void (*callback)(void *, void *, multi_heap_info_t *));
    LISA_LOGI(LOG_TAG, "%10s %10s %12s %12s %12s %12s %12s %12s %12s\n", "[Start]", "[End]", "[Alloc/BK]", "[Free/BK]", "[Total/BK]",
          "[MaxFree/BK]", "[Alloc/B]", "[Free/B]", "[MinFree/B]");
    heap_caps_travel(heap_travel_cb);
}

////////////////////////////////////////////////////////////////////////////////
void *_malloc_r(struct _reent *_r, size_t size)
{
    return heap_caps_malloc_default(size);
}
void *_realloc_r(struct _reent *_r, void *ptr, size_t size)
{
    return heap_caps_realloc_default(ptr, size);
}
void *_calloc_r(struct _reent *_r, size_t num, size_t size)
{
    return heap_caps_calloc(num, size, MALLOC_CAP_DEFAULT);
}
void _free_r(struct _reent *_r, void *ptr)
{
    return heap_caps_free(ptr);
}

////////////////////////////////////////////////////////////////////////////////
void *inram_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}
void *inram_malloc(size_t align, size_t size)
{
    return heap_caps_aligned_alloc(align, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}
void *inram_calloc(size_t align, size_t num, size_t size)
{
    return heap_caps_aligned_calloc(align, num, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}
void inram_free(void *ptr)
{
    return heap_caps_free(ptr);
}

////////////////////////////////////////////////////////////////////////////////
void *exram_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}
void *exram_malloc(size_t align, size_t size)
{
    return heap_caps_aligned_alloc(align, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}
void *exram_calloc(size_t align, size_t num, size_t size)
{
    return heap_caps_aligned_calloc(align, num, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}
void exram_free(void *ptr)
{
    return heap_caps_free(ptr);
}

#if CONFIG_PSRAM_HEAP
void *psram_realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}

void *psram_malloc_align(size_t align, size_t size)
{
    return heap_caps_aligned_alloc(align, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}

void *psram_malloc(size_t size)
{
    return psram_malloc_align(4, size);
}

void *psram_calloc_align(size_t align, size_t num, size_t size)
{
    return heap_caps_aligned_calloc(align, num, size, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
}

void *psram_calloc(size_t num, size_t size)
{
    return psram_calloc_align(4, num, size);
}

void psram_free(void *ptr)
{
    return heap_caps_free(ptr);
}
#endif

////////////////////////////////////////////////////////////////////////////////

#define CRASH_ON_ALLOC_FAIL 1
static void heap_caps_failed_alloc_callback(size_t size, uint32_t caps, const char *func_name)
{
    printf("Failed to allocate %u bytes in %s\n", size, func_name);

    heap_summary_info();

#if CRASH_ON_ALLOC_FAIL
    __builtin_trap();
#endif
}

void sysheap_init(void)
{
    extern uint32_t __psram_heap_start;
    extern uint32_t __psram_heap_end;
    extern void scatload_psram(void);

    (void)__psram_heap_start;  // Suppress unused variable warning
    (void)__psram_heap_end;    // Suppress unused variable warning

    heap_caps_init();

    heap_caps_register_failed_alloc_callback(heap_caps_failed_alloc_callback);

    /* register PSRAM heap */
#if CONFIG_PSRAM_HEAP
    const uint32_t psram_caps[] = {
        MEM_BASE_CAPS | MALLOC_CAP_SPIRAM,
        MEM_BASE_CAPS | MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT,
        0,
    };
    heap_caps_add_region_with_caps(psram_caps, (intptr_t)psram_default_heap,
                                   (intptr_t)psram_default_heap + sizeof(psram_default_heap));
#endif

#if CONFIG_PSRAM_NOCACHE_HEAP
    if (((uintptr_t)psram_nocache_heap & (CONFIG_PSRAM_NOCACHE_HEAP_SIZE - 1)) != 0) {
        printf("Error: psram_nocache_heap address 0x%p is not aligned to size 0x%x\n",
               (void*)psram_nocache_heap, CONFIG_PSRAM_NOCACHE_HEAP_SIZE);
    }
    else{
        const uint32_t nocache_caps[] = {
            MEM_BASE_CAPS | MALLOC_CAP_NOCACHE,
            MEM_BASE_CAPS | MALLOC_CAP_NOCACHE | MALLOC_CAP_SPIRAM,
            MEM_BASE_CAPS | MALLOC_CAP_NOCACHE | MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT,
        };
        device_region_enable((uint32_t)psram_nocache_heap,sizeof(psram_nocache_heap));
        heap_caps_add_region_with_caps(nocache_caps, (intptr_t)psram_nocache_heap,
                                   (intptr_t)psram_nocache_heap + sizeof(psram_nocache_heap));
    }

#endif
    heap_caps_enable_nonos_stack_heaps();
    heap_caps_malloc_extmem_enable(1024);
}

#endif
