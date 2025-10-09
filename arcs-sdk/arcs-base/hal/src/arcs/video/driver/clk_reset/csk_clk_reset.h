#ifndef _CSK_CLK_RESET_H_
#define _CSK_CLK_RESET_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chip.h"
#include "ClockManager.h"
#include "log_print.h"
#include "csk_timer.h"
#include "mmio.h"
#include "csk_driver.h"

#include "csk_clk_reset.h"

/************** AP_CFG RESET ************************************************/
void ap_cfg_gpdma_reset(void);
void ap_cfg_video_reset(void);
void ap_cfg_vic_reset(void);
void ap_cfg_qspi0_reset(void);
void ap_cfg_qspi1_reset(void);
void ap_cfg_rgb_reset(void);
void ap_cfg_blender_reset(void);
void ap_cfg_jpeg_reset(void);

/************** AP_CFG CLK ENABLE ************************************************/
void ap_cfg_gpdma_clk_enable(void);
void ap_cfg_video_clk_enable(void);
void ap_cfg_vic_clk_enable(void);
void ap_cfg_rgb_clk_enable(void);
void ap_cfg_qspi0_clk_enable(void);
void ap_cfg_qspi1_clk_enable(void);
void ap_cfg_blender_clk_enable(void);
void ap_cfg_jpeg_clk_enable(void);

/************** AP_CFG CLK DISABLE ************************************************/
void ap_cfg_gpdma_clk_disable(void);
void ap_cfg_video_clk_disable(void);
void ap_cfg_vic_clk_disable(void);
void ap_cfg_rgb_clk_disable(void);
void ap_cfg_qspi0_clk_disable(void);
void ap_cfg_qspi1_clk_disable(void);
void ap_cfg_blender_clk_disable(void);
void ap_cfg_jpeg_clk_disable(void);

/************** AP_CFG CLK SEL ************************************************/
void ap_cfg_vic_clk_inv(void);
void ap_cfg_rgb_clk_sel_24(void);
void ap_cfg_rgb_clk_sel_sys(void);
void ap_cfg_rgb_clk_div_m(uint8_t div);
void ap_cfg_rgb_clk_inv(void);
void ap_cfg_rgb_clk_normal(void);
void ap_cfg_rgb_clk_ld(void);
void ap_cfg_qspi0_clk_sel(void);
void ap_cfg_qspi0_clk_div_m(uint8_t div);
void ap_cfg_qspi0_clk_div_n(uint8_t div);
void ap_cfg_qspi0_clk_inv(void);
void ap_cfg_qspi1_clk_sel(void);
void ap_cfg_qspi1_clk_div_m(uint8_t div);
void ap_cfg_qspi1_clk_div_n(uint8_t div);
void ap_cfg_qspi1_clk_inv(void);
void ap_cfg_reg_dump(void);

void ap_cfg_dma_sel_dvp(void);
void ap_cfg_dma_sel_qspi_in(void);
void ap_cfg_dma_sel_rgb(void);
void ap_cfg_dma_sel_qspi_out(void);

#if 0
/************** image proc ************************************************/
void image_proc_enable(void);
void image_proc_vic_enable(void);
void image_proc_jpeg_enable(void);
void image_proc_blender_enable(void);
void image_proc_display_enable(void);
void image_proc_qspi_enable(void);
void image_proc_rgb_enable(void);
void image_proc_disable(void);
void image_proc_vic_disable(void);
void image_proc_jpeg_disable(void);
void image_proc_blender_disable(void);
void image_proc_display_disable(void);
void image_proc_qspi_disable(void);
void image_proc_rgb_disable(void);
void image_proc_sel_dvp(void);
void image_proc_sel_qspi(void);
void image_proc_reg_dump(void);
#endif

#endif
