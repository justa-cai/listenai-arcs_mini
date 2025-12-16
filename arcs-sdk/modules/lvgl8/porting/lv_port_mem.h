/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORT_GUI_LVGL_MEM_H_
#define LV_PORT_MEM_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int lvgl_port_mem_init(void);
void *lvgl_port_malloc(size_t size);
void *lvgl_port_realloc(void *ptr, size_t size);
void lvgl_port_free(void *ptr);

/**
 * @brief Get LVGL heap statistics
 * @param total Total heap size
 * @param alloc_cnt Allocation count
 * @param free_cnt Free count
 * @param failed_cnt Failed allocation count
 */
void lvgl_port_mem_get_stats(size_t *total, size_t *alloc_cnt, size_t *free_cnt, size_t *failed_cnt);

/**
 * @brief Print LVGL heap statistics to log
 */
void lvgl_port_mem_print_stats(void);

/**
 * @brief Start LVGL heap monitor thread (prints stats every 5 seconds)
 */
void lvgl_heap_monitor_start(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GUI_LVGL_MEM_H_ */
