/*
 * Intentionally no include guard: this file is included once per benchmark instance.
 */

#ifndef BENCHMARK_BIND_FN
#error "BENCHMARK_BIND_FN must be defined"
#endif

#ifndef BENCHMARK_START_FN
#error "BENCHMARK_START_FN must be defined"
#endif

#ifndef BENCHMARK_CLOSE_FN
#error "BENCHMARK_CLOSE_FN must be defined"
#endif

#ifndef BENCHMARK_RUN_SCENE_FN
#error "BENCHMARK_RUN_SCENE_FN must be defined"
#endif

#ifndef BENCHMARK_SET_FINISHED_CB_FN
#error "BENCHMARK_SET_FINISHED_CB_FN must be defined"
#endif

#ifndef BENCHMARK_SET_MAX_SPEED_FN
#error "BENCHMARK_SET_MAX_SPEED_FN must be defined"
#endif

#include "lvgl.h"

static lv_disp_t *benchmark_bound_disp;

void BENCHMARK_BIND_FN(lv_disp_t *disp)
{
    benchmark_bound_disp = disp;
}

static lv_disp_t *benchmark_get_bound_disp(void)
{
    return benchmark_bound_disp != NULL ? benchmark_bound_disp : lv_disp_get_default();
}

#define lv_demo_benchmark BENCHMARK_START_FN
#define lv_demo_benchmark_close BENCHMARK_CLOSE_FN
#define lv_demo_benchmark_run_scene BENCHMARK_RUN_SCENE_FN
#define lv_demo_benchmark_set_finished_cb BENCHMARK_SET_FINISHED_CB_FN
#define lv_demo_benchmark_set_max_speed BENCHMARK_SET_MAX_SPEED_FN
#define lv_disp_get_default() benchmark_get_bound_disp()
#define lv_scr_act() lv_disp_get_scr_act(benchmark_get_bound_disp())

#include "../../../../../../modules/lvgl8/demos/benchmark/lv_demo_benchmark.c"

#undef lv_demo_benchmark
#undef lv_demo_benchmark_close
#undef lv_demo_benchmark_run_scene
#undef lv_demo_benchmark_set_finished_cb
#undef lv_demo_benchmark_set_max_speed
#undef lv_disp_get_default
#undef lv_scr_act

#undef BENCHMARK_BIND_FN
#undef BENCHMARK_START_FN
#undef BENCHMARK_CLOSE_FN
#undef BENCHMARK_RUN_SCENE_FN
#undef BENCHMARK_SET_FINISHED_CB_FN
#undef BENCHMARK_SET_MAX_SPEED_FN
