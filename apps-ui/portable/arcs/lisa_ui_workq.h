#ifndef __LISA_UI_WORKQ_H__
#define __LISA_UI_WORKQ_H__

#include <stdint.h>

typedef void *lisa_ui_workq_t;

typedef void (*lisa_ui_workq_worker_t)(void *arg, uint32_t arg_len);

lisa_ui_workq_t lisa_ui_workq_create(const char *name, uint32_t stack_size, uint32_t prio, uint32_t q_cnt);

int lisa_ui_workq_submit(lisa_ui_workq_t workq, lisa_ui_workq_worker_t worker, void *arg, uint32_t arg_len);

int lisa_ui_workq_submit_delay_ms(lisa_ui_workq_t workq, lisa_ui_workq_worker_t worker, void *arg, uint32_t arg_len,
                                  uint32_t delay_ms);

#endif
