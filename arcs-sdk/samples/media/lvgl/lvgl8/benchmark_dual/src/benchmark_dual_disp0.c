/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define BENCHMARK_BIND_FN lv_demo_benchmark_disp0_bind
#define BENCHMARK_START_FN lv_demo_benchmark_disp0
#define BENCHMARK_CLOSE_FN lv_demo_benchmark_disp0_close
#define BENCHMARK_RUN_SCENE_FN lv_demo_benchmark_disp0_run_scene
#define BENCHMARK_SET_FINISHED_CB_FN lv_demo_benchmark_disp0_set_finished_cb
#define BENCHMARK_SET_MAX_SPEED_FN lv_demo_benchmark_disp0_set_max_speed

#include "benchmark_dual_instance_impl.h"
