#include "lv_port_mem.h"
#if CONFIG_LV_DEMO_SYS_HEAP
#include "sysheap.h"
#else
#include "xutils.h"
#endif

#include "esp_heap_caps.h"
#include "multi_heap.h"
#include "lisa_log.h"

#define TAG "lvgl_heap"

#define MEM_BASE_CAPS (MALLOC_CAP_32BIT | MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_EXEC)

/* 使用Kconfig配置LVGL heap大小 */
#if CONFIG_LV_DEMO_SYS_HEAP
#ifdef CONFIG_LVGL_HEAP_SIZE
#define LVGL_HEAP_SIZE CONFIG_LVGL_HEAP_SIZE
#else
#define LVGL_HEAP_SIZE (1 * 1024 * 1024)  /* 默认1MB */
#endif

static uint8_t lvgl_port_mem[LVGL_HEAP_SIZE] __attribute__((section(".psram.bss")));
#endif

/* 内存统计 */
static struct {
    size_t total_size;
    size_t alloc_count;
    size_t free_count;
    size_t realloc_count;
    size_t failed_count;
    size_t peak_used;       /* 历史最大使用量 */
    size_t min_free;        /* 历史最小剩余量 */
} lvgl_mem_stats = {0};

int lvgl_port_mem_init(void)
{
#if CONFIG_LV_DEMO_SYS_HEAP
    static bool initialized = false;
    
    /* 防止重复初始化 */
    if (initialized) {
        LISA_LOGW(TAG, "LVGL heap already initialized, skipping");
        return 0;
    }
    
    const uint32_t psram_caps[] = {
        MEM_BASE_CAPS | MALLOC_CAP_PID2,
        MEM_BASE_CAPS | MALLOC_CAP_PID2,
        0,
    };
    
    /* 初始化内存统计 */
    lvgl_mem_stats.total_size = LVGL_HEAP_SIZE;
    lvgl_mem_stats.alloc_count = 0;
    lvgl_mem_stats.free_count = 0;
    lvgl_mem_stats.realloc_count = 0;
    lvgl_mem_stats.failed_count = 0;
    lvgl_mem_stats.peak_used = 0;
    lvgl_mem_stats.min_free = LVGL_HEAP_SIZE;  /* 初始为总大小 */
    
    /* 添加LVGL独立heap到heap_caps系统 */
    heap_caps_add_region_with_caps(psram_caps, (intptr_t)lvgl_port_mem,
                                   (intptr_t)lvgl_port_mem + sizeof(lvgl_port_mem));
    
    initialized = true;
    
    LISA_LOGI(TAG, "LVGL heap initialized: %d bytes at 0x%p", LVGL_HEAP_SIZE, lvgl_port_mem);
    
    return 0;
#else
    LISA_LOGW(TAG, "LVGL independent heap is disabled");
    return -1;
#endif
}

void *lvgl_port_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_PID2);
    
    if (ptr) {
        lvgl_mem_stats.alloc_count++;
        
        /* 更新历史峰值统计 */
        size_t free_size = heap_caps_get_free_size(MALLOC_CAP_PID2);
        size_t used_size = lvgl_mem_stats.total_size - free_size;
        
        if (used_size > lvgl_mem_stats.peak_used) {
            lvgl_mem_stats.peak_used = used_size;
        }
        
        if (free_size < lvgl_mem_stats.min_free) {
            lvgl_mem_stats.min_free = free_size;
        }
    } else {
        lvgl_mem_stats.failed_count++;
        
        /* 获取heap状态用于调试 */
        size_t free_size = heap_caps_get_free_size(MALLOC_CAP_PID2);
        size_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_PID2);
        
        LISA_LOGE(TAG, "malloc FAILED: size=%d, free=%d, largest_free=%d, alloc=%d, free=%d, failed=%d",
                  size, free_size, largest_free,
                  lvgl_mem_stats.alloc_count, lvgl_mem_stats.free_count,
                  lvgl_mem_stats.failed_count);
        
        /* 打印详细heap统计 */
        lvgl_port_mem_print_stats();
    }

    return ptr;
}

void *lvgl_port_realloc(void *ptr, size_t size)
{
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_PID2);
    
    if (new_ptr) {
        lvgl_mem_stats.realloc_count++;
        
        /* 更新历史峰值统计 */
        size_t free_size = heap_caps_get_free_size(MALLOC_CAP_PID2);
        size_t used_size = lvgl_mem_stats.total_size - free_size;
        
        if (used_size > lvgl_mem_stats.peak_used) {
            lvgl_mem_stats.peak_used = used_size;
        }
        
        if (free_size < lvgl_mem_stats.min_free) {
            lvgl_mem_stats.min_free = free_size;
        }
    } else {
        lvgl_mem_stats.failed_count++;
        LISA_LOGW(TAG, "realloc failed: size=%d", size);
    }
    
    return new_ptr;
}

void lvgl_port_free(void *ptr)
{
    if (ptr) {
        lvgl_mem_stats.free_count++;
        heap_caps_free(ptr);
    }
}

/* 获取LVGL heap统计信息 */
void lvgl_port_mem_get_stats(size_t *total, size_t *alloc_cnt, size_t *free_cnt, size_t *failed_cnt)
{
    if (total) *total = lvgl_mem_stats.total_size;
    if (alloc_cnt) *alloc_cnt = lvgl_mem_stats.alloc_count;
    if (free_cnt) *free_cnt = lvgl_mem_stats.free_count;
    if (failed_cnt) *failed_cnt = lvgl_mem_stats.failed_count;
}

/* 打印LVGL heap统计信息 */
void lvgl_port_mem_print_stats(void)
{
    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_PID2);
    size_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_PID2);
    size_t used_size = lvgl_mem_stats.total_size - free_size;
    
    LISA_LOGI(TAG, "========== LVGL Heap Statistics ==========");
    LISA_LOGI(TAG, "Total size:     %d bytes (%.2f MB)", 
              lvgl_mem_stats.total_size, lvgl_mem_stats.total_size / (1024.0 * 1024.0));
    LISA_LOGI(TAG, "Used size:      %d bytes (%.2f MB, %.1f%%)", 
              used_size, used_size / (1024.0 * 1024.0), 
              (used_size * 100.0) / lvgl_mem_stats.total_size);
    LISA_LOGI(TAG, "Free size:      %d bytes (%.2f MB, %.1f%%)", 
              free_size, free_size / (1024.0 * 1024.0),
              (free_size * 100.0) / lvgl_mem_stats.total_size);
    LISA_LOGI(TAG, "Largest free:   %d bytes (%.2f KB)", 
              largest_free, largest_free / 1024.0);
    LISA_LOGI(TAG, "--- Historical Peak Statistics ---");
    LISA_LOGI(TAG, "Peak used:      %d bytes (%.2f MB, %.1f%%)", 
              lvgl_mem_stats.peak_used, lvgl_mem_stats.peak_used / (1024.0 * 1024.0),
              (lvgl_mem_stats.peak_used * 100.0) / lvgl_mem_stats.total_size);
    LISA_LOGI(TAG, "Min free:       %d bytes (%.2f KB)", 
              lvgl_mem_stats.min_free, lvgl_mem_stats.min_free / 1024.0);
    LISA_LOGI(TAG, "--- Operation Statistics ---");
    LISA_LOGI(TAG, "Alloc count:    %d", lvgl_mem_stats.alloc_count);
    LISA_LOGI(TAG, "Free count:     %d", lvgl_mem_stats.free_count);
    LISA_LOGI(TAG, "Realloc count:  %d", lvgl_mem_stats.realloc_count);
    LISA_LOGI(TAG, "Failed count:   %d", lvgl_mem_stats.failed_count);
    LISA_LOGI(TAG, "=========================================");
}

/* LVGL heap monitor thread (temporary debug) */
#include "FreeRTOS.h"
#include "task.h"

static void lvgl_heap_monitor_task(void *arg)
{
    (void)arg;
    
    LISA_LOGI(TAG, "LVGL heap monitor task started");
    
    while (1) {
        /* Sleep for 5 seconds */
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        /* Print heap statistics */
        lvgl_port_mem_print_stats();
    }
}

void lvgl_heap_monitor_start(void)
{
    BaseType_t ret;
    
    ret = xTaskCreate(lvgl_heap_monitor_task,
                      "lvgl_heap_mon",
                      2048,  /* Stack size in words */
                      NULL,
                      5,     /* Priority */
                      NULL);
    
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "Failed to create LVGL heap monitor task");
    }
}
