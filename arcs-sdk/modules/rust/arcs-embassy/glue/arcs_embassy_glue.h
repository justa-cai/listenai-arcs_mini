/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ARCS_EMBASSY_GLUE_H
#define ARCS_EMBASSY_GLUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t arcs_embassy_now_us(void);
uint8_t arcs_embassy_cs_acquire(void);
void arcs_embassy_cs_release(uint8_t state);
void *arcs_embassy_cur_task(void);
void arcs_embassy_notify(void *task);
uint32_t arcs_embassy_notify_take(uint32_t clear, uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif /* ARCS_EMBASSY_GLUE_H */
