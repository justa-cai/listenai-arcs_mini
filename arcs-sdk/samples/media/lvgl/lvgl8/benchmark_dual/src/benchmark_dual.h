/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BENCHMARK_DUAL_H
#define BENCHMARK_DUAL_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void lv_demo_benchmark_disp0_bind(lv_disp_t *disp);
void lv_demo_benchmark_disp0(void);

void lv_demo_benchmark_disp1_bind(lv_disp_t *disp);
void lv_demo_benchmark_disp1(void);

#ifdef __cplusplus
}
#endif

#endif /* BENCHMARK_DUAL_H */
