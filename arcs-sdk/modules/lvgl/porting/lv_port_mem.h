/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PORT_GUI_LVGL_MEM_H_
#define PORT_GUI_LVGL_MEM_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void *lvgl_port_malloc(size_t size);

void *lvgl_port_realloc(void *ptr, size_t size);

void lvgl_port_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GUI_LVGL_MEM_H_ */
