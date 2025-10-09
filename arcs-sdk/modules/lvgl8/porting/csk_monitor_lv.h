#ifndef CSK_MONITOR_LV_H
#define CSK_MONITOR_LV_H

#include <stdint.h>
#include "core_feature_base.h"
#include "ClockManager.h"

#if CONFIG_LV_CUSTOM_MONITOR
extern volatile uint64_t g_dma2d_fill_total_time;
extern volatile uint64_t g_dma2d_fill_dma_time;
extern volatile uint32_t g_dma2d_fill_call_cnt;

extern volatile uint64_t g_dma2d_copy_total_time;
extern volatile uint64_t g_dma2d_copy_dma_time;
extern volatile uint32_t g_dma2d_copy_call_cnt;

extern volatile uint64_t g_draw_letter_time;
extern volatile uint32_t g_draw_letter_call_cnt;

extern volatile uint64_t g_draw_image_time;
extern volatile uint32_t g_draw_image_call_cnt;

extern volatile uint64_t g_draw_rect_time;
extern volatile uint32_t g_draw_rect_call_cnt;

#ifdef CONFIG_LV_USE_RPC
extern volatile uint64_t g_rpc_fill_normal_total_time;
extern volatile uint64_t g_rpc_fill_normal_proxy_time;
extern volatile uint32_t g_rpc_fill_normal_call_cnt;

extern volatile uint64_t g_rpc_map_normal_total_time;
extern volatile uint64_t g_rpc_map_normal_proxy_time;
extern volatile uint32_t g_rpc_map_normal_call_cnt;

extern volatile uint64_t g_rpc_fill_blended_total_time;
extern volatile uint64_t g_rpc_fill_blended_proxy_time;
extern volatile uint32_t g_rpc_fill_blended_call_cnt;

extern volatile uint64_t g_rpc_map_blended_total_time;
extern volatile uint64_t g_rpc_map_blended_proxy_time;
extern volatile uint32_t g_rpc_map_blended_call_cnt;
#endif

#endif

#define _CORE_CLK 300000000

static inline uint64_t get_time_cycle(void)
{ 
    return __get_rv_cycle();
}

static inline uint64_t calc_time_elapsed_cycle(uint64_t cycle)
{
	return __get_rv_cycle() - cycle;
}

static inline uint32_t clk_2_us(uint64_t clk)
{
    return (uint32_t)(clk * 1000 * 1000 / _CORE_CLK);
}

#endif