#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "chip.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "PSRAMManager.h"
#include "Driver_GPDMA.h"
#include "Driver_DMA2D.h"
#include "Driver_Blender.h"

#include "csk_dma2d_wrap.h"


#define __CONIFG_TAKE_TIME      0
#define __CONIFG_CONFIG_REG     1
#define __CONIFG_TAKE_CYCLE     0
#define __CONIFG_REG_DUMP       1

static uint32_t reg_cfg_start = 0;
static uint32_t reg_cfg_end = 0;
static uint32_t dma_start = 0;
static uint32_t dma_end = 0;
static uint32_t total_start = 0;
static uint32_t total_end = 0;


static void dma2d_config_dump(lvgl_dma2d_cfg_t *pcfg);

static inline uint32_t get_time_us(void)
{
#if 0
    uint64_t cur_tick = SysTimer_GetLoadValue();
    return (uint32_t)(cur_tick * 1000 * 1000 / CRM_GetMtimeFreq());
#else
    //return __get_rv_cycle();
    return __RV_CSR_READ(CSR_MCYCLE);
#endif
}

static inline uint32_t calc_time_elapsed_us(uint32_t start_time)
{
#if 0
    return get_time_us() - start_time;
#else
    //return __get_rv_cycle() - start_time;
    return __RV_CSR_READ(CSR_MCYCLE) - start_time;
#endif
}




#if __CONIFG_CONFIG_REG

volatile static uint32_t dma2d_finish_flag = 0;

__attribute__((section(".itcm.text")))
 void CSK_DMA2D_IRQ_Handler(void){
    volatile uint32_t int_status = IP_DMA2D->REG_DMA_IMAGE_INT_STATUS.all;

    // Clear interrupt pending
    IP_DMA2D->REG_DMA_IMAGE_INT_CLR.all = int_status;

	//VIDEO_LOG("[%s:%d] int_status = 0x%x", __func__, __LINE__, int_status);

    if(int_status & (1<<3))
    {
        dma2d_finish_flag = 1;
        //VIDEO_LOG("[%s:%d] ch9 out", __func__, __LINE__);
    }

//    if(int_status & (1<<2))
//    {
//        VIDEO_LOG("[%s:%d] ch8 mask", __func__, __LINE__);
//    }
//
//    if(int_status & (1<<1))
//    {
//        VIDEO_LOG("[%s:%d] ch7 fore", __func__, __LINE__);
//    }
//
//    if(int_status & (1<<0))
//    {
//        VIDEO_LOG("[%s:%d] ch6 back", __func__, __LINE__);
//    }
}


int32_t rgb565_dma2d_init(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable
    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;

    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    disable_IRQ(IRQ_DMAC_GP_IMG_VECTOR);
    register_ISR(IRQ_DMAC_GP_IMG_VECTOR, CSK_DMA2D_IRQ_Handler, NULL);
    enable_IRQ(IRQ_DMAC_GP_IMG_VECTOR);

    IP_GPDMA->REG_DMA_IMAGE_INT_EN.all = 1;                     // ch6~9 CFG_IMAGE_BLOCK_FINISH_INT_EN
    IP_GPDMA->REG_DMA_IMAGE_FEATURE_CTRL.all = (1 << 31);       // (0xF << 28);     // ch6~9 LEFT_UP

    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH6 = 2;  // 2:word
    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH7 = 2;  // 2:word
    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH8 = 2;  // 2:word
    IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.bit.CFG_TRANS_DST_BASE_UNIT_CH9 = 2;  // 2:word

    VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    return ret;
}


/*
#define BLENDER_BACK_DMA_CH6   dma_2d_ch6
#define BLENDER_FORE_DMA_CH7   dma_2d_ch7
#define BLENDER_MASK_DMA_CH8   dma_2d_ch8
#define BLENDER_OUT_DMA_CH9    dma_2d_ch9
*/
#define DMA2D_BACK_BURST_LEN         dma2d_burst_len_8spl
#define DMA2D_FORE_BURST_LEN         dma2d_burst_len_8spl
#define DMA2D_MASK_BURST_LEN         dma2d_burst_len_4spl
#define DMA2D_OUT_BURST_LEN          dma2d_burst_len_8spl

static inline void dma2d_ch6_back_unstep_fix(uint32_t buf_addr, uint32_t size_word)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH6_CTRL.all = (d2back_hs_num3 << 28) | (1 << 21) | (1 << 20) | (1 << 8) | (DMA2D_BACK_BURST_LEN << 16) | (DMA2D_BACK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_fix << 9) | (gpdma_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH6.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = D2BACK_BUF;

    /* GPDMA start */
    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x1;
    IP_GPDMA->REG_DMA_CH6_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch6_back_step_fix(uint32_t buf_addr, uint32_t size_word, uint16_t buf_w, uint16_t copy_w, uint16_t copy_h)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH6_CTRL.all = (d2back_hs_num3 << 28) | (1 << 21) | (1 << 20) | (1 << 19) | (1 << 8) | (DMA2D_BACK_BURST_LEN << 16) | (DMA2D_BACK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_fix << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH6.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = D2BACK_BUF;

    /* DMA2D step config */
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all = (1 << 29) | ((copy_w >> 1) << 16) | copy_h;  // ((pcfg->copy_w * 2 / sizeof(uint32_t)) << 16) | pcfg->copy_h;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.all = ((buf_w - copy_w) << 1) + 4;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.all = 0x10001;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all &= 0xE;
    IP_GPDMA->REG_DMA_CH6_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch6_back_unstep(uint32_t buf_addr, uint32_t size_word)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH6_CTRL.all = (d2back_hs_num3 << 28) | (1 << 21) | (1 << 20) | (1 << 8) | (DMA2D_BACK_BURST_LEN << 16) | (DMA2D_BACK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (gpdma_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH6.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = D2BACK_BUF;

    /* GPDMA start */
    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x1;
    IP_GPDMA->REG_DMA_CH6_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch6_back_step(uint32_t buf_addr, uint32_t size_word, uint16_t buf_w, uint16_t copy_w, uint16_t copy_h)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH6_CTRL.all = (d2back_hs_num3 << 28) | (1 << 21) | (1 << 20) | (1 << 19) | (1 << 8) | (DMA2D_BACK_BURST_LEN << 16) | (DMA2D_BACK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH6.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH6.all = D2BACK_BUF;

    /* DMA2D step config */
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all = size_word;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all = (1 << 29) | ((copy_w >> 1) << 16) | copy_h;  // ((pcfg->copy_w * 2 / sizeof(uint32_t)) << 16) | pcfg->copy_h;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.all = ((buf_w - copy_w) << 1) + 4;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.all = 0x10001;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all &= 0xE;
    IP_GPDMA->REG_DMA_CH6_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch7_fore_unstep(uint32_t buf_addr, uint32_t size_word)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH7_CTRL.all = (d2fore_hs_num4 << 28) | (1 << 21) | (1 << 20) | (1 << 8) | (DMA2D_FORE_BURST_LEN << 16) | (DMA2D_FORE_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH7.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH7.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH7.all = D2FORE_BUF;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x2;
    IP_GPDMA->REG_DMA_CH7_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch7_fore_step(uint32_t buf_addr, uint32_t size_word, uint16_t buf_w, uint16_t copy_w, uint16_t copy_h)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH7_CTRL.all = (d2fore_hs_num4 << 28) | (1 << 21) | (1 << 20) | (1 << 19) | (1 << 8) | (DMA2D_FORE_BURST_LEN << 16) | (DMA2D_FORE_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH7.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH7.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH7.all = D2FORE_BUF;

    /* DMA2D step config */
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all = size_word;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all = (1 << 29) | ((copy_w >> 1) << 16) | copy_h;  // ((pcfg->copy_w * 2 / sizeof(uint32_t)) << 16) | pcfg->copy_h;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.all = ((buf_w - copy_w) << 1) + 4;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.all = 0x10001;

    /* GPDMA start */
    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all &= 0xD;
    IP_GPDMA->REG_DMA_CH7_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch8_mask_unstep(uint32_t buf_addr, uint32_t size_word)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH8_CTRL.all = (d2mask_hs_num2 << 28) | (1 << 21) | (1 << 20) | (1 << 8) | (DMA2D_MASK_BURST_LEN << 16) | (DMA2D_MASK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH8.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH8.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH8.all = D2MASK_BUF;
    //IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all = 1 << 30;
    //IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all = size_word;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x4;
    IP_GPDMA->REG_DMA_CH8_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch8_mask_step(uint32_t buf_addr, uint32_t size_word, uint16_t buf_w, uint16_t copy_w, uint16_t copy_h)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH8_CTRL.all = (d2mask_hs_num2 << 28) | (1 << 21) | (1 << 20) | (1 << 8) | (DMA2D_MASK_BURST_LEN << 16) | (DMA2D_MASK_BURST_LEN << 14) | \
                                    (inc_mode_fix << 10) | (inc_mode_increase << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_m2p << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH8.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH8.all = buf_addr;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH8.all = D2MASK_BUF;

    /* DMA2D step config */
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all = size_word;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all = (1 << 29) | ((copy_w >> 1) << 16) | copy_h;  // ((pcfg->copy_w * 2 / sizeof(uint32_t)) << 16) | pcfg->copy_h;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.all = ((buf_w - copy_w) << 1) + 4;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.all = 0x10001;

    /* GPDMA start */
    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all &= 0xB;
    IP_GPDMA->REG_DMA_CH8_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch9_out_unstep(uint32_t buf_addr, uint32_t size_word)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH9_CTRL.all = (d2out_hs_num1 << 28) | (1 << 21) | (1 << 8) | (DMA2D_OUT_BURST_LEN << 16) | (DMA2D_OUT_BURST_LEN << 14) | \
                                    (inc_mode_increase << 10) | (inc_mode_fix << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_p2m << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH9.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH9.all = D2OUT_BUF;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH9.all = buf_addr;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x8;
    IP_GPDMA->REG_DMA_DEC_OUT2D_BYPASS.all |= 0x8;
    IP_GPDMA->REG_DMA_CH9_CTRL.all |= (1 << 1);
}

static inline void dma2d_ch9_out_step(uint32_t buf_addr, uint32_t size_word, uint16_t buf_w, uint16_t copy_w, uint16_t copy_h)
{
    /* GPDMA normal config */
    IP_GPDMA->REG_DMA_CH9_CTRL.all = (d2out_hs_num1 << 28) | (1 << 21) | (1 << 19) | (1 << 8) | (DMA2D_OUT_BURST_LEN << 16) | (DMA2D_OUT_BURST_LEN << 14) | \
                                    (inc_mode_increase << 10) | (inc_mode_fix << 9) | (dma2d_sample_unit_word << 6) | (tfr_mode_p2m << 4) | (1 << 0);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH9.all = size_word;
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH9.all = D2OUT_BUF;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH9.all = buf_addr;

    /* DMA2D step config */
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.all = (copy_h << 16) | copy_w;
    IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.all = size_word;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all = (1 << 29) | ((copy_w >> 1) << 16) | copy_h;  // ((pcfg->copy_w * 2 / sizeof(uint32_t)) << 16) | pcfg->copy_h;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.all = ((((buf_w - copy_w) << 1) + 4) << 16) | 4;
    IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.all = (copy_h << 16) | 1;

    /* GPDMA start */
    //IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all |= 0x8;
    //IP_GPDMA->REG_DMA_DEC_OUT2D_BYPASS.all &= 0x7;
    IP_GPDMA->REG_DMA_CH9_CTRL.all |= (1 << 1);
}

 __attribute__((section(".itcm.text")))
 int32_t rgb565_dma2d_config(lvgl_dma2d_cfg_t *pcfg)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    //uint32_t back_size_word = 0;
    //uint32_t fore_size_word = 0;
    uint32_t mask_size_word = 0;
    uint32_t out_size_word = 0;
    uint32_t back_color = 0;
    //bool is_give_mutex = false;
    
    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);
    //dma2d_config_dump(pcfg);

#if __CONIFG_TAKE_TIME
    uint32_t started = get_time_us();
#endif

#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    extern SemaphoreHandle_t gpdma_mutex;

	if (xSemaphoreTake(gpdma_mutex, portMAX_DELAY) != pdTRUE) {
		VIDEO_LOG("[%s] Failed to take TE semaphore", __FUNCTION__);
		return -1;
	}
#endif
#if __CONIFG_TAKE_TIME
    //uint32_t mutexed = calc_time_elapsed_us(started);
#endif

#if __CONIFG_TAKE_CYCLE
	reg_cfg_start = __RV_CSR_READ(CSR_MCYCLE);
#endif

    out_size_word = pcfg->copy_w * pcfg->copy_h >> 1;  // * 2 / 4
    mask_size_word = pcfg->copy_w * pcfg->copy_h >> 2;
    IP_D2BLENDER->REG_BLENDER_EN.all = 0x0;     // bit0:reg_clk_force_on  bit1:BLENDER_EN
    IP_D2BLENDER->REG_FIFO_BURST_THD.all = (4 << 24) | (8 << 16) | (8 << 8) | 8;   // burst_thd=8   mask:out:back:fore
    IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all = out_size_word;
    IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all = out_size_word;
    IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all = mask_size_word;
    IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS.all = 0xE;
    IP_GPDMA->REG_DMA_DEC_OUT2D_BYPASS.all = 0x7;

    /* blender init */
    switch(pcfg->mode)
    {
        /* back + color=0xFFFFFF  alpha=0x00  => out */
        case LVGL_DMA2D_COPY:
            IP_D2BLENDER->REG_BLENDER_CTRL.all = (BLENDER_MODE_FILL << 7) | (BLENDER_FORE_FORMAT_RGB565 << 4) | (BLENDER_BACK_FORMAT_RGB565 << 2) | (BLENDER_ALPHA_MODE_2);  // 0x04
            IP_D2BLENDER->REG_ALPHA.all = 0;                                        // 0x08
            IP_D2BLENDER->REG_COLOR.all = 0xFFFFFF;                                        // 0x0C
            if(pcfg->map_w == pcfg->copy_w) {
                dma2d_ch6_back_unstep((uint32_t)pcfg->map, out_size_word);
            } else {
                dma2d_ch6_back_step((uint32_t)pcfg->map, out_size_word, pcfg->map_w, pcfg->copy_w, pcfg->copy_h);
            }
            break;

        /* back(fix) + color  alpha=0xFF  => out */
        case LVGL_DMA2D_FILL:
            IP_D2BLENDER->REG_BLENDER_CTRL.all = (BLENDER_MODE_FILL << 7) | (BLENDER_FORE_FORMAT_RGB565 << 4) | (BLENDER_BACK_FORMAT_RGB565 << 2) | (BLENDER_ALPHA_MODE_2);  // 0x04
            IP_D2BLENDER->REG_ALPHA.all = 0xFF;                                        // 0x08
            IP_D2BLENDER->REG_COLOR.all = pcfg->color;                                        // 0x0C

            if(pcfg->buf_w == pcfg->copy_w) {
                dma2d_ch6_back_unstep_fix((uint32_t)(&back_color), out_size_word);
            } else {
                dma2d_ch6_back_step_fix((uint32_t)(&back_color), out_size_word, pcfg->buf_w, pcfg->copy_w, pcfg->copy_h);
            }
            break;

        /* back(fix) + color  alpha=mask+opa  => out */
        case LVGL_DMA2D_FILL_MASK:
            IP_D2BLENDER->REG_BLENDER_CTRL.all = (BLENDER_MODE_FILL << 7) | (BLENDER_FORE_FORMAT_RGB565 << 4) | (BLENDER_BACK_FORMAT_RGB565 << 2) | (BLENDER_ALPHA_MODE_1);  // 0x04
            IP_D2BLENDER->REG_ALPHA.all = pcfg->opa;                                        // 0x08
            IP_D2BLENDER->REG_COLOR.all = pcfg->color;                                        // 0x0C

            if(pcfg->buf_w == pcfg->copy_w) {
                dma2d_ch6_back_unstep((uint32_t)pcfg->buf, out_size_word);
            } else {
                dma2d_ch6_back_step((uint32_t)pcfg->buf, out_size_word, pcfg->buf_w, pcfg->copy_w, pcfg->copy_h);
            }

            if(pcfg->mask_w == pcfg->copy_w) {
                dma2d_ch8_mask_unstep((uint32_t)pcfg->mask, mask_size_word);
            } else {
                /* error */
                dma2d_ch8_mask_step((uint32_t)pcfg->mask, mask_size_word, pcfg->mask_w, pcfg->copy_w, pcfg->copy_h);
            }
            break;

        /* back + fore  alpha=opa  => out */
        case LVGL_DMA2D_BLEND:
            IP_D2BLENDER->REG_BLENDER_CTRL.all = (BLENDER_MODE_MAP << 7) | (BLENDER_FORE_FORMAT_RGB565 << 4) | (BLENDER_BACK_FORMAT_RGB565 << 2) | (BLENDER_ALPHA_MODE_2);  // 0x04
            IP_D2BLENDER->REG_ALPHA.all = pcfg->opa;                                        // 0x08
            IP_D2BLENDER->REG_COLOR.all = 0xFFFFFF;                                        // 0x0C

            if(pcfg->buf_w == pcfg->copy_w) {
                dma2d_ch6_back_unstep((uint32_t)pcfg->buf, out_size_word);
            } else {
                dma2d_ch6_back_step((uint32_t)pcfg->buf, out_size_word, pcfg->buf_w, pcfg->copy_w, pcfg->copy_h);
            }

            if(pcfg->map_w == pcfg->copy_w) {
                dma2d_ch7_fore_unstep((uint32_t)pcfg->map, out_size_word);
            } else {
                dma2d_ch7_fore_step((uint32_t)pcfg->map, out_size_word, pcfg->map_w, pcfg->copy_w, pcfg->copy_h);
            }
            break;

        /* back + fore  alpha=mask+opa  => out */
        case LVGL_DMA2D_BLEND_MASK:
            IP_D2BLENDER->REG_BLENDER_CTRL.all = (BLENDER_MODE_MAP << 7) | (BLENDER_FORE_FORMAT_RGB565 << 4) | (BLENDER_BACK_FORMAT_RGB565 << 2) | (BLENDER_ALPHA_MODE_1);  // 0x04
            IP_D2BLENDER->REG_ALPHA.all = pcfg->opa;                                        // 0x08
            IP_D2BLENDER->REG_COLOR.all = 0xFFFFFF;                                        // 0x0C

            if(pcfg->buf_w == pcfg->copy_w) {
                dma2d_ch6_back_unstep((uint32_t)pcfg->buf, out_size_word);
            } else {
                dma2d_ch6_back_step((uint32_t)pcfg->buf, out_size_word, pcfg->buf_w, pcfg->copy_w, pcfg->copy_h);
            }

            if(pcfg->map_w == pcfg->copy_w) {
                dma2d_ch7_fore_unstep((uint32_t)pcfg->map, out_size_word);
            } else {
                dma2d_ch7_fore_step((uint32_t)pcfg->map, out_size_word, pcfg->map_w, pcfg->copy_w, pcfg->copy_h);
            }

            if(pcfg->mask_w == pcfg->copy_w) {
                dma2d_ch8_mask_unstep((uint32_t)pcfg->mask, mask_size_word);
            } else {
                dma2d_ch8_mask_step((uint32_t)pcfg->mask, mask_size_word, pcfg->mask_w, pcfg->copy_w, pcfg->copy_h);
            }
            break;

        default:
            break;
    }

    dma2d_ch9_out_step((uint32_t)pcfg->buf, out_size_word, pcfg->buf_w, pcfg->copy_w, pcfg->copy_h);

#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    xSemaphoreGive(gpdma_mutex);
    is_give_mutex = true;
#endif

#if __CONIFG_REG_DUMP
    lvgl_gpdma_reg_dump();
    lvgl_blender_reg_dump();
#endif

    /* blender start */
    dma2d_finish_flag = 0;

#if __CONIFG_TAKE_CYCLE
    dma_start = __RV_CSR_READ(CSR_MCYCLE);
#endif
    IP_D2BLENDER->REG_BLENDER_EN.all = (1 << 1);

#if __CONIFG_TAKE_TIME
    uint32_t dma_start = get_time_us();
    uint32_t errored = 0;
#endif

#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
	if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(20)) != pdTRUE) {
#if __CONIFG_TAKE_TIME
        errored = calc_time_elapsed_us(start);
#endif
        VIDEO_LOG("[%s:%d] wait timeout INT_STATUS=0x%x", __func__, __LINE__, IP_DMA2D->REG_DMA_IMAGE_INT_STATUS.all);
        // dma2d_config_dump(pcfg);
        // lvgl_gpdma_reg_dump();
        // lvgl_blender_reg_dump();
        ret = FAILURE;
        goto error;
	}
#else
    timeout = 10000000;
    while(!dma2d_finish_flag)
    {
        //DELAY_US(1);
        if(timeout-- == 0)
        {
//            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
//            dma2d_config_dump(pcfg);
//            lvgl_gpdma_reg_dump();
//            lvgl_blender_reg_dump();
            ret = FAILURE;
            goto error;
        }
    }
#endif

#if __CONIFG_TAKE_CYCLE
    dma_end = __RV_CSR_READ(CSR_MCYCLE);
#endif

    // if((pcfg->mode == LVGL_DMA2D_BLEND_MASK) && (pcfg->buf_w == 640) && (pcfg->map_w == 72) && (pcfg->mask_w == 72) && (pcfg->copy_w == 72) && (pcfg->copy_h == 8))
    // {
    //     VIDEO_LOG("[%s:%d] success", __func__, __LINE__);
    //     dma2d_config_dump(pcfg);
    // }

#if __CONIFG_TAKE_TIME
    uint32_t dma_done = calc_time_elapsed_us(dma_start);
#endif

    ret = SUCCESS;

error:
#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    if (is_give_mutex == false) {
        xSemaphoreGive(gpdma_mutex);
    }
#endif

    /* blender stop */
    IP_D2BLENDER->REG_BLENDER_EN.all = 0x0;
    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;

    /* GPDMA stop and clear */
    IP_GPDMA->REG_DMA_CH6_CTRL.all = 0xC;
    IP_GPDMA->REG_DMA_CH7_CTRL.all = 0xC;
    IP_GPDMA->REG_DMA_CH8_CTRL.all = 0xC;
    IP_GPDMA->REG_DMA_CH9_CTRL.all = 0xC;
    IP_DMA2D->REG_DMA_CH_CLR.all = (0xF << 6);  // clear fifo
    //IP_GPDMA->REG_DMA_IMAGE_INT_CLR.all = (1<<3);

#if __CONIFG_TAKE_TIME
    uint32_t finished = calc_time_elapsed_us(started);
    if ( errored ) {
        VIDEO_LOG("error, dma=%d, code=%d, used=%d", errored, finished - errored, finished);
    }else{
        VIDEO_LOG("done, dma=%d, code=%d, used=%d", dma_done, finished - dma_done, finished);
    }
#endif
    // VIDEO_LOG("func:%s, mode:%d, buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d\n", __FUNCTION__, pcfg->mode, pcfg->buf, pcfg->map, pcfg->mask, pcfg->buf_w, pcfg->map_w, pcfg->copy_w, pcfg->copy_h, pcfg->color, pcfg->opa);

    //VIDEO_LOG("[%s:%d]", __func__, __LINE__);

#if __CONIFG_TAKE_CYCLE
    reg_cfg_end = __RV_CSR_READ(CSR_MCYCLE);
#endif

    return ret;
}

#else

 volatile uint32_t blender_dma_finish_flag = 0;

 void d2blender_out_dma_callback(uint32_t event, void* workspace)
{
    // VIDEO_LOG("[%s:%d] event=%d", __func__, __LINE__, event);
    blender_dma_finish_flag++;
//	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//	xSemaphoreGiveFromISR(dma_sem, &xHigherPriorityTaskWoken);
//	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


 void DMA2D_Step_Config(csk_dma2d_ch_t dma_ch, csk_dma2d_format_t format, uint16_t src_width, uint16_t dts_width, uint16_t height)
{
    uint8_t pixel_byte = 0;

    switch(format)
    {
        case DMA2D_FORMAT_RGB565:
            pixel_byte = RGB565_PIXEL_BYTE;
            break;

        case DMA2D_FORMAT_RGB888:
            pixel_byte = RGB888_PIXEL_BYTE;
            break;

        default:
            break;
    }

    switch(dma_ch)
    {
        case dma_2d_ch6:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_L0_CH6 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN6 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN6 = 1;
            break;

        case dma_2d_ch7:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x2;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_L0_CH7 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN7 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN7 = 1;
            break;

        case dma_2d_ch8:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_L0_CH8 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN8 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN8 = 1;
            break;

        case dma_2d_ch9:
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= 0x8;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_L0_CH9 = ((dts_width - src_width) * pixel_byte) + 4;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
            IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = height;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_RIGHT_DOWN_EN9 = 0;
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.bit.CFG_MEMCOPY_LEFT_UP_EN9 = 1;
            break;

        default:
            break;
    }
}

int32_t rgb565_dma2d_init(void)
{
    int32_t ret = FAILURE;

    VIDEO_LOG("DMA2D INIT\n");

    __HAL_CRM_VIDEO_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG1.bit.ENA_BLENDER_CLK = 0x1; // blender clk enable

    // ret = GPDMA_Initialize();
    // CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    /* gpdma 2d init */
    ret = DMA2D_Initialize();
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

	// Create binary semaphore for DMA synchronization
//	dma_sem = xSemaphoreCreateBinary();
//	if (dma_sem == NULL) {
//		VIDEO_LOG("[%s] Failed to create DMA semaphore", __FUNCTION__);
//		return -1;
//	}

    // xSemaphoreGive(dma_sem);

    return ret;
}


#define D2BLENDER_BACK_GPDMA_CH   gp_dma_ch4
#define D2BLENDER_MASK_GPDMA_CH   gp_dma_ch5

#define D2BLENDER_BACK_DMA_CH   dma_2d_ch6
#define D2BLENDER_FORE_DMA_CH   dma_2d_ch7
#define D2BLENDER_MASK_DMA_CH   dma_2d_ch8
#define D2BLENDER_OUT_DMA_CH    dma_2d_ch9

 int32_t rgb565_dma2d_config(lvgl_dma2d_cfg_t *pcfg)
{
    int32_t ret = FAILURE;
    uint32_t timeout = 0;
    uint32_t back_size_byte = 0;
    uint32_t fore_size_byte = 0;
    uint32_t mask_size_byte = 0;
    uint32_t out_size_byte = 0;
    uint32_t back_color = 0;
    void *back_buf = NULL;
    bool is_give_mutex = false;
#if __CONIFG_TAKE_TIME
    uint32_t started = get_time_us();
#endif

#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    extern SemaphoreHandle_t gpdma_mutex;

	if (xSemaphoreTake(gpdma_mutex, portMAX_DELAY) != pdTRUE) {
		VIDEO_LOG("[%s] Failed to take TE semaphore", __FUNCTION__);
		return -1;
	}
#endif
#if __CONIFG_TAKE_TIME
    uint32_t mutexed = calc_time_elapsed_us(started);
#endif
    void *blender_dev = Blender0();

    static Blender_InitTypeDef blender_cfg = {
            .blender_mode = BLENDER_MODE_FILL,
            .alpha_mode = BLENDER_ALPHA_MODE_2,
            .back_format = BLENDER_BACK_FORMAT_RGB565,
            .fore_format = BLENDER_FORE_FORMAT_RGB565,
            .img_width = 0,
            .img_height = 0,
            .color = 0xFFFFFF,
            .alpha = 0,
            .burst_thd = 8,
    };

     static csk_gpdma_init_t gpdma_back_input = {
            .dma_ch = D2BLENDER_BACK_GPDMA_CH,
            .burst_len = gpdma_burst_len_2spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_fix,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2back_hs_num3,
    };

     static csk_dma2d_init_t d2back_input = {
        .dma_ch = D2BLENDER_BACK_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2back_hs_num3,
    };

     static csk_dma_2d_image_cfg_t d2back_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

     static csk_dma2d_init_t d2fore_input = {
        .dma_ch = D2BLENDER_FORE_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2fore_hs_num4,
    };

     static csk_dma_2d_image_cfg_t d2fore_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

     static csk_gpdma_init_t gpdma_mask_input = {
            .dma_ch = D2BLENDER_MASK_GPDMA_CH,
            .burst_len = gpdma_burst_len_2spl,
            .src_mode = address_mode_normal,
            .dst_mode = address_mode_normal,
            .tfr_mode = tfr_mode_m2p,
            .src_inc_mode = inc_mode_increase,
            .dst_inc_mode = inc_mode_fix,
            .prio_lvl = prio_mode_vhigh,
            .sample_unit = gpdma_sample_unit_word,
            .handshake = d2mask_hs_num2,
     };

     static csk_dma2d_init_t d2mask_input = {
        .dma_ch = D2BLENDER_MASK_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2p,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_fix,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2mask_hs_num2,
    };

     static csk_dma_2d_image_cfg_t d2mask_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

     static csk_dma2d_init_t d2out_output = {
        .dma_ch = D2BLENDER_OUT_DMA_CH,
        .burst_len = dma2d_burst_len_2spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .sample_unit = dma2d_sample_unit_word,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .rd_done_ack = read_done_ack_enable,
        .handshake = d2out_hs_num1,
    };

     static csk_dma_2d_image_cfg_t d2out_img_cfg = {
        .img_input_format = csk_image_format_yuv422,
        .img_width = 0,
        .img_height = 0,
        .start_col = (0+1),
        .start_row = (0+1),
        .end_col = 0,
        .end_row = 0,
        .img_output_fromat_transfer = csk_image_format_transfer_yuv422_crop,
        .img_yuv422_format = csk_image_yuv422_format_y0cby1cr,
    };

    /* reset and clear */
    IP_AP_CFG->REG_SW_RESET.bit.BLENDER_RESET = 0x1;
    IP_GPDMA->REG_DMA_CH5_CTRL.all |= 0xC;
    IP_GPDMA->REG_DMA_CH6_CTRL.all |= 0xC;
    IP_GPDMA->REG_DMA_CH7_CTRL.all |= 0xC;
    IP_GPDMA->REG_DMA_CH8_CTRL.all |= 0xC;
    IP_GPDMA->REG_DMA_CH9_CTRL.all |= 0xC;
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (0x1F << 5);

    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
            blender_cfg.blender_mode = BLENDER_MODE_FILL;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_2;
            blender_cfg.img_width = pcfg->copy_w;
            blender_cfg.img_height = pcfg->copy_h;
            blender_cfg.color = 0xFFFFFF;
            blender_cfg.alpha = 0;
            back_buf = pcfg->map;

            d2back_img_cfg.img_width = pcfg->map_w;
            d2back_img_cfg.img_height = pcfg->copy_h;
            d2back_img_cfg.end_col = pcfg->copy_w;
            d2back_img_cfg.end_row = pcfg->copy_h;

            back_size_byte = pcfg->map_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            break;

        case LVGL_DMA2D_FILL:
            blender_cfg.blender_mode = BLENDER_MODE_FILL;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_2;
            blender_cfg.img_width = pcfg->copy_w;
            blender_cfg.img_height = pcfg->copy_h;
            blender_cfg.color = pcfg->color;
            blender_cfg.alpha = 0xFF;
            back_buf = &back_color;
            //back_buf = pcfg->buf;

            d2back_img_cfg.img_width = pcfg->buf_w;
            d2back_img_cfg.img_height = pcfg->copy_h;
            d2back_img_cfg.end_col = pcfg->copy_w;
            d2back_img_cfg.end_row = pcfg->copy_h;

            back_size_byte = pcfg->copy_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            break;

        case LVGL_DMA2D_FILL_MASK:
            blender_cfg.blender_mode = BLENDER_MODE_FILL;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
            blender_cfg.img_width = pcfg->copy_w;
            blender_cfg.img_height = pcfg->copy_h;
            blender_cfg.color = pcfg->color;
            blender_cfg.alpha = pcfg->opa;
            back_buf = pcfg->buf;

            d2back_img_cfg.img_width = pcfg->buf_w;
            d2back_img_cfg.img_height = pcfg->copy_h;
            d2back_img_cfg.end_col = pcfg->copy_w;
            d2back_img_cfg.end_row = pcfg->copy_h;

            d2mask_img_cfg.img_width = pcfg->mask_w;
            d2mask_img_cfg.img_height = pcfg->copy_h;
            d2mask_img_cfg.end_col = pcfg->copy_w;
            d2mask_img_cfg.end_row = pcfg->copy_h;

            mask_size_byte = pcfg->mask_w * pcfg->copy_h;
            back_size_byte = pcfg->copy_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            break;

        case LVGL_DMA2D_BLEND:
            blender_cfg.blender_mode = BLENDER_MODE_MAP;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_2;
            blender_cfg.img_width = pcfg->copy_w;
            blender_cfg.img_height = pcfg->copy_h;
            blender_cfg.color = 0xFFFFFF;
            blender_cfg.alpha = pcfg->opa;
            back_buf = pcfg->buf;

            d2back_img_cfg.img_width = pcfg->buf_w;
            d2back_img_cfg.img_height = pcfg->copy_h;
            d2back_img_cfg.end_col = pcfg->copy_w;
            d2back_img_cfg.end_row = pcfg->copy_h;

            d2fore_img_cfg.img_width = pcfg->map_w;
            d2fore_img_cfg.img_height = pcfg->copy_h;
            d2fore_img_cfg.end_col = pcfg->copy_w;
            d2fore_img_cfg.end_row = pcfg->copy_h;

            back_size_byte = pcfg->buf_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            fore_size_byte = pcfg->map_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            break;

        case LVGL_DMA2D_BLEND_MASK:
            blender_cfg.blender_mode = BLENDER_MODE_MAP;
            blender_cfg.alpha_mode = BLENDER_ALPHA_MODE_1;
            blender_cfg.img_width = pcfg->copy_w;
            blender_cfg.img_height = pcfg->copy_h;
            blender_cfg.color = 0xFFFFFF;
            blender_cfg.alpha = pcfg->opa;
            back_buf = pcfg->buf;

            d2back_img_cfg.img_width = pcfg->buf_w;
            d2back_img_cfg.img_height = pcfg->copy_h;
            d2back_img_cfg.end_col = pcfg->copy_w;
            d2back_img_cfg.end_row = pcfg->copy_h;

            d2fore_img_cfg.img_width = pcfg->map_w;
            d2fore_img_cfg.img_height = pcfg->copy_h;
            d2fore_img_cfg.end_col = pcfg->copy_w;
            d2fore_img_cfg.end_row = pcfg->copy_h;

            d2mask_img_cfg.img_width = pcfg->mask_w;
            d2mask_img_cfg.img_height = pcfg->copy_h;
            d2mask_img_cfg.end_col = pcfg->copy_w;
            d2mask_img_cfg.end_row = pcfg->copy_h;

            back_size_byte = pcfg->buf_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            fore_size_byte = pcfg->map_w * pcfg->copy_h * RGB565_PIXEL_BYTE;
            mask_size_byte = pcfg->mask_w * pcfg->copy_h;
            break;

        default:
            break;
    }

    d2out_img_cfg.img_width = pcfg->buf_w;
    d2out_img_cfg.img_height = pcfg->copy_h;
    d2out_img_cfg.end_col = pcfg->copy_w;
    d2out_img_cfg.end_row = pcfg->copy_h;
    out_size_byte = pcfg->copy_w * pcfg->copy_h * RGB565_PIXEL_BYTE;

    /* blender init */
    ret = Blender_Initialize(blender_dev, &blender_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    /* GPDMA init and start */
    blender_dma_finish_flag = 0;
    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
            ret = DMA2D_Config(&d2back_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            break;

        case LVGL_DMA2D_FILL:
#if 0
            ret = GPDMA_Config(&gpdma_back_input, NULL, NULL);
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);

            ret = GPDMA_Start_Normal(gpdma_back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ(ret, CSK_DRIVER_OK);
#else
            ret = DMA2D_Config(&d2back_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
#endif
            break;

        case LVGL_DMA2D_FILL_MASK:
            ret = DMA2D_Config(&d2back_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            if(pcfg->mask_w == pcfg->copy_w) {
                ret = GPDMA_Config(&gpdma_mask_input, NULL, NULL);
                CHECK_RET_EQ(ret, CSK_DRIVER_OK);

                ret = GPDMA_Start_Normal(gpdma_mask_input.dma_ch, pcfg->mask, (uint32_t*)D2MASK_BUF, mask_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            } else {
                ret = DMA2D_Config(&d2mask_input, NULL, NULL);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

                ret = DMA2D_Image_Config_Extend(d2mask_input.dma_ch, &d2mask_img_cfg);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

                ret = DMA2D_Start_Normal(d2mask_input.dma_ch, pcfg->mask, (uint32_t*)D2MASK_BUF, mask_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            }
            break;


        case LVGL_DMA2D_BLEND:
            ret = DMA2D_Config(&d2back_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Config(&d2fore_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2fore_input.dma_ch, &d2fore_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2fore_input.dma_ch, pcfg->map, (uint32_t*)D2FORE_BUF, fore_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            break;

        case LVGL_DMA2D_BLEND_MASK:
            ret = DMA2D_Config(&d2back_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2back_input.dma_ch, &d2back_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Config(&d2fore_input, NULL, NULL);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Image_Config_Extend(d2fore_input.dma_ch, &d2fore_img_cfg);
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2back_input.dma_ch, back_buf, (uint32_t*)D2BACK_BUF, back_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            ret = DMA2D_Start_Normal(d2fore_input.dma_ch, pcfg->map, (uint32_t*)D2FORE_BUF, fore_size_byte / sizeof(uint32_t));
            CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

            if(pcfg->mask_w == pcfg->copy_w) {
                ret = GPDMA_Config(&gpdma_mask_input, NULL, NULL);
                CHECK_RET_EQ(ret, CSK_DRIVER_OK);

                ret = GPDMA_Start_Normal(gpdma_mask_input.dma_ch, pcfg->mask, (uint32_t*)D2MASK_BUF, mask_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ(ret, CSK_DRIVER_OK);
            } else {
                ret = DMA2D_Config(&d2mask_input, NULL, NULL);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

                ret = DMA2D_Image_Config_Extend(d2mask_input.dma_ch, &d2mask_img_cfg);
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

                ret = DMA2D_Start_Normal(d2mask_input.dma_ch, pcfg->mask, (uint32_t*)D2MASK_BUF, mask_size_byte / sizeof(uint32_t));
                CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);
            }

            break;
            
        default:
            break;
    }

    ret = DMA2D_Config(&d2out_output, d2blender_out_dma_callback, NULL);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    ret = DMA2D_Image_Config_Extend(d2out_output.dma_ch, &d2out_img_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    DMA2D_Step_Config(d2out_output.dma_ch, DMA2D_FORMAT_RGB565, pcfg->copy_w, pcfg->buf_w, pcfg->copy_h);

    ret = DMA2D_Start_Normal(d2out_output.dma_ch, (uint32_t*)D2OUT_BUF, pcfg->buf, out_size_byte / sizeof(uint32_t));
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    xSemaphoreGive(gpdma_mutex);
    is_give_mutex = true;
#endif

//    if((pcfg->mode == LVGL_DMA2D_BLEND_MASK) && (pcfg->buf_w == 640) && (pcfg->map_w == 72) && (pcfg->mask_w == 72) && (pcfg->copy_w == 72) && (pcfg->copy_h == 8))
//    {
//        dma2d_config_dump(pcfg);
//        lvgl_gpdma_reg_dump();
//        lvgl_blender_reg_dump();
//        //while(1);
//    }

//    lvgl_gpdma_reg_dump();
//    lvgl_blender_reg_dump();

    /* blender start */
    Blender_Start(blender_dev);

#if __CONIFG_TAKE_TIME
    uint32_t start = get_time_us();
    uint32_t errored = 0;
#endif

#if 0
	if (xSemaphoreTake(dma_sem, pdMS_TO_TICKS(20)) != pdTRUE) {
#if __CONIFG_TAKE_TIME
        errored = calc_time_elapsed_us(start);
#endif
        goto error;
	}
#else
    timeout = 10000000;
    while(!blender_dma_finish_flag)
    {
        //DELAY_US(1);
        if(timeout-- == 0)
        {
//            VIDEO_LOG("[%s:%d] wait timeout", __func__, __LINE__);
//            dma2d_config_dump(pcfg);
//            lvgl_gpdma_reg_dump();
//            lvgl_blender_reg_dump();
            ret = FAILURE;
            goto error;
        }
    }
#endif

#if __CONIFG_TAKE_TIME
    uint32_t ela = calc_time_elapsed_us(start);
#endif

    // Blender_Stop(blender_dev);

    ret = SUCCESS;

error:
#if CONFIG_LVGL_GPU_CSK_GPDMA_DMA2D_MUTEX
    if (is_give_mutex == false) {
        xSemaphoreGive(gpdma_mutex);
    }
#endif

    switch(pcfg->mode)
    {
        case LVGL_DMA2D_COPY:
            DMA2D_Stop(d2back_input.dma_ch);
            DMA2D_Stop(d2out_output.dma_ch);
            break;

        case LVGL_DMA2D_FILL:
            GPDMA_Stop(gpdma_back_input.dma_ch);
            DMA2D_Stop(d2out_output.dma_ch);
            break;

        case LVGL_DMA2D_FILL_MASK:
            GPDMA_Stop(gpdma_back_input.dma_ch);
            if(pcfg->mask_w == pcfg->copy_w) {
                GPDMA_Stop(gpdma_mask_input.dma_ch);
            } else {
                DMA2D_Stop(d2mask_input.dma_ch);
            }
            DMA2D_Stop(d2out_output.dma_ch);
            break;

        case LVGL_DMA2D_BLEND:
            DMA2D_Stop(d2fore_input.dma_ch);
            DMA2D_Stop(d2back_input.dma_ch);
            DMA2D_Stop(d2out_output.dma_ch);
            break;

        case LVGL_DMA2D_BLEND_MASK:
            DMA2D_Stop(d2fore_input.dma_ch);
            DMA2D_Stop(d2back_input.dma_ch);
            if(pcfg->mask_w == pcfg->copy_w) {
                GPDMA_Stop(gpdma_mask_input.dma_ch);
            } else {
                DMA2D_Stop(d2mask_input.dma_ch);
            }
            DMA2D_Stop(d2out_output.dma_ch);
            break;

        default:
            break;
    }

    Blender_Stop(blender_dev);

#if __CONIFG_TAKE_TIME
    uint32_t finished = calc_time_elapsed_us(started);
    if ( errored ) {
        VIDEO_LOG("[%s:%d] jun wait mode[%d] error, mutex=%dus, dma=%dus, code=%dus, used=%dus (copy_w:%d, copy_h:%d = %d)", __func__, __LINE__, pcfg->mode, mutexed, errored, finished - errored, finished, pcfg->copy_w, pcfg->copy_h, pcfg->copy_w * pcfg->copy_h);
    }else{
        VIDEO_LOG("[%s:%d] jun wait mode[%d] done, mutex=%dus, dma=%dus, code=%dus, used=%dus (copy_w:%d, copy_h:%d = %d)", __func__, __LINE__, pcfg->mode, mutexed, ela, finished - ela, finished, pcfg->copy_w, pcfg->copy_h, pcfg->copy_w * pcfg->copy_h);
    }
#endif
    //VIDEO_LOG("func:%s, mode:%d, buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d", __FUNCTION__, pcfg->mode, pcfg->buf, pcfg->map, pcfg->mask, pcfg->buf_w, pcfg->map_w, pcfg->copy_w, pcfg->copy_h, pcfg->color, pcfg->opa);

    return ret;
}
#endif


int32_t lv_gpu_dma2d_copy(void *buf, uint16_t buf_w, void *map, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};

    lvgl_cfg.mode = LVGL_DMA2D_COPY;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = map;
    lvgl_cfg.copy_w = copy_w;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = map_w;
    lvgl_cfg.copy_h = copy_h;
    lvgl_cfg.color = 0xFFFFFF;
    lvgl_cfg.opa = 0xFF;

    // VIDEO_LOG("func: %s ", __FUNCTION__);

    //dma2d_config_dump(&lvgl_cfg);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    ret = rgb565_dma2d_config(&lvgl_cfg);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
#if __CONIFG_TAKE_CYCLE
    //dma2d_config_dump(&lvgl_cfg);
    VIDEO_LOG("cycle: total=%d dma=%d reg=%d", total_end - total_start, dma_end - dma_start, (reg_cfg_end - reg_cfg_start) - (dma_end - dma_start));
    //VIDEO_LOG("ns: total=%d dma=%d reg=%d", (total_end - total_start) * 10 / 3, (dma_end - dma_start) * 10 / 3, ((reg_cfg_end - reg_cfg_start) - (dma_end - dma_start)) * 10 / 3);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

error:
    if(ret != CSK_DRIVER_OK)
    {
         dma2d_config_dump(&lvgl_cfg);
    }

    return ret;
}


 int32_t lv_gpu_dma2d_fill(void *buf, uint16_t buf_w, uint32_t color, uint16_t fill_w, uint16_t fill_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};
    uint32_t rgb888 = 0;

#if 0
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13)) << 16) | \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8 ) | \
             (((color & 0x001F) << 3) | ((color & 0x001F) >> 2));
#elif 0
    rgb888 = (((color & 0xF800) << 8) | ((color & 0x07E0) << 5 ) | ((color & 0x001F) << 3));
#else
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13))) |         /* R: 楂�5浣嶈浆鍒颁綆8浣� */ \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8) |     /* G: 涓棿6浣嶈浆鍒颁腑闂�8浣� */ \
             ((((color & 0x001F) << 3) | ((color & 0x001F) >> 2)) << 16);     /* B: 浣�5浣嶈浆鍒伴珮8浣� */
#endif

    lvgl_cfg.buf = buf;
    lvgl_cfg.mode = LVGL_DMA2D_FILL;
    // lvgl_cfg.buf = temp_buf;
    lvgl_cfg.map = NULL;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = 0;
    lvgl_cfg.copy_w = fill_w;
    lvgl_cfg.copy_h = fill_h;
    lvgl_cfg.color = rgb888;    // RGB888
    lvgl_cfg.opa = 0xFF;

    // VIDEO_LOG("func:%s,mode:%d, buf:0x%x, temp_buf:0x%x, map:0x%x, mask:0x%x, buf_w:%d, map_w:%d, fill_w:%d, copy_w:%d, copy_h:%d, color:0x%x, opa:%d\n",
    //         __FUNCTION__, lvgl_cfg.mode, buf, lvgl_cfg.buf, lvgl_cfg.map, lvgl_cfg.mask, lvgl_cfg.buf_w, lvgl_cfg.map_w, fill_w,lvgl_cfg.copy_w, lvgl_cfg.copy_h, lvgl_cfg.color, lvgl_cfg.opa);

    // VIDEO_LOG("func: %s ", __FUNCTION__);

    //dma2d_config_dump(&lvgl_cfg);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    ret = rgb565_dma2d_config(&lvgl_cfg);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
#if __CONIFG_TAKE_CYCLE
    //dma2d_config_dump(&lvgl_cfg);
    VIDEO_LOG("cycle: total=%d dma=%d reg=%d", total_end - total_start, dma_end - dma_start, (reg_cfg_end - reg_cfg_start) - (dma_end - dma_start));
    //VIDEO_LOG("ns: total=%d dma=%d reg=%d", (total_end - total_start) * 10 / 3, (dma_end - dma_start) * 10 / 3, ((reg_cfg_end - reg_cfg_start) - (dma_end - dma_start)) * 10 / 3);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

error:
    if(ret != CSK_DRIVER_OK)
    {
         dma2d_config_dump(&lvgl_cfg);
    }

    return ret;
}


int32_t lv_gpu_dma2d_fill_mask(void *buf, uint16_t buf_w, uint32_t color, void *mask, uint16_t mask_w, uint8_t opa, uint16_t fill_w, uint16_t fill_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};
    uint32_t rgb888 = 0;

#if 0
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13)) << 16) | \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8 ) | \
             (((color & 0x001F) << 3) | ((color & 0x001F) >> 2));
#elif 0
    rgb888 = (((color & 0xF800) << 8) | ((color & 0x07E0) << 5 ) | ((color & 0x001F) << 3));
#else
    rgb888 = ((((color & 0xF800) >> 8) | ((color & 0xF800) >> 13))) |         /* R: 楂�5浣嶈浆鍒颁綆8浣� */ \
             ((((color & 0x07E0) >> 3) | ((color & 0x07E0) >> 9)) << 8) |     /* G: 涓棿6浣嶈浆鍒颁腑闂�8浣� */ \
             ((((color & 0x001F) << 3) | ((color & 0x001F) >> 2)) << 16);     /* B: 浣�5浣嶈浆鍒伴珮8浣� */
#endif

    lvgl_cfg.mode = LVGL_DMA2D_FILL_MASK;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = NULL;
    lvgl_cfg.mask = mask;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = 0;
    lvgl_cfg.mask_w = mask_w;
    //lvgl_cfg.mask_w = buf_w;
    lvgl_cfg.copy_w = fill_w;
    lvgl_cfg.copy_h = fill_h;
    lvgl_cfg.color = rgb888;    // RGB888
    lvgl_cfg.opa = opa;

    // VIDEO_LOG("func: %s ", __FUNCTION__);

    //uint32_t started = get_time_us();
    //dma2d_config_dump(&lvgl_cfg);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    ret = rgb565_dma2d_config(&lvgl_cfg);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
#if __CONIFG_TAKE_CYCLE
    //dma2d_config_dump(&lvgl_cfg);
    VIDEO_LOG("cycle: total=%d dma=%d reg=%d", total_end - total_start, dma_end - dma_start, (reg_cfg_end - reg_cfg_start) - (dma_end - dma_start));
    //VIDEO_LOG("ns: total=%d dma=%d reg=%d", (total_end - total_start) * 10 / 3, (dma_end - dma_start) * 10 / 3, ((reg_cfg_end - reg_cfg_start) - (dma_end - dma_start)) * 10 / 3);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //uint32_t finished = calc_time_elapsed_us(started);

    //VIDEO_LOG("[%s:%d] all=%dus (copy_w:%d, copy_h:%d = %d)", __func__, __LINE__, finished, lvgl_cfg.copy_w, lvgl_cfg.copy_h, lvgl_cfg.copy_w * lvgl_cfg.copy_h);

error:
    if(ret != CSK_DRIVER_OK)
    {
         dma2d_config_dump(&lvgl_cfg);
    }

    return ret;
}


 int32_t lv_gpu_dma2d_blend(void *buf, uint16_t buf_w, void *map, uint8_t opa, uint16_t map_w, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};

    if(copy_h == 0) {
        VIDEO_LOG("func: %s, line: %d, copy_h == 0", __FUNCTION__, __LINE__);
        return -1;
    }

    lvgl_cfg.mode = LVGL_DMA2D_BLEND;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = map;
    lvgl_cfg.mask = NULL;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = map_w;
    lvgl_cfg.copy_w = copy_w;
    lvgl_cfg.copy_h = copy_h;
    lvgl_cfg.color = 0xFFFFFF;
    lvgl_cfg.opa = opa;

    // VIDEO_LOG("func: %s ", __FUNCTION__);

    //dma2d_config_dump(&lvgl_cfg);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    ret = rgb565_dma2d_config(&lvgl_cfg);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
#if __CONIFG_TAKE_CYCLE
    //dma2d_config_dump(&lvgl_cfg);
    VIDEO_LOG("cycle: total=%d dma=%d reg=%d", total_end - total_start, dma_end - dma_start, (reg_cfg_end - reg_cfg_start) - (dma_end - dma_start));
    //VIDEO_LOG("ns: total=%d dma=%d reg=%d", (total_end - total_start) * 10 / 3, (dma_end - dma_start) * 10 / 3, ((reg_cfg_end - reg_cfg_start) - (dma_end - dma_start)) * 10 / 3);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

error:
    if(ret != CSK_DRIVER_OK)
    {
         dma2d_config_dump(&lvgl_cfg);
    }

    return ret;
}


 int32_t lv_gpu_dma2d_blend_mask(void *buf, uint16_t buf_w, void *map, uint16_t map_w, void *mask, uint16_t mask_w, uint8_t opa, uint16_t copy_w, uint16_t copy_h)
{
    int32_t ret = FAILURE;
    lvgl_dma2d_cfg_t lvgl_cfg = {0};

    if(copy_h == 0) {
        VIDEO_LOG("func: %s, line: %d, copy_h == 0", __FUNCTION__, __LINE__);
        return -1;
    }

    lvgl_cfg.mode = LVGL_DMA2D_BLEND_MASK;
    lvgl_cfg.buf = buf;
    lvgl_cfg.map = map;
    lvgl_cfg.mask = mask;
    lvgl_cfg.buf_w = buf_w;
    lvgl_cfg.map_w = map_w;
    lvgl_cfg.mask_w = mask_w;
    lvgl_cfg.copy_w = copy_w;
    lvgl_cfg.copy_h = copy_h;
    lvgl_cfg.color = 0xFFFFFF;
    lvgl_cfg.opa = opa;

    // VIDEO_LOG("func: %s ", __FUNCTION__);

    //uint32_t started = get_time_us();

    //dma2d_config_dump(&lvgl_cfg);

    total_start = __RV_CSR_READ(CSR_MCYCLE);
    ret = rgb565_dma2d_config(&lvgl_cfg);
    total_end = __RV_CSR_READ(CSR_MCYCLE);
#if __CONIFG_TAKE_CYCLE
    //dma2d_config_dump(&lvgl_cfg);
    VIDEO_LOG("cycle: total=%d dma=%d reg=%d", total_end - total_start, dma_end - dma_start, (reg_cfg_end - reg_cfg_start) - (dma_end - dma_start));
    //VIDEO_LOG("ns: total=%d dma=%d reg=%d", (total_end - total_start) * 10 / 3, (dma_end - dma_start) * 10 / 3, ((reg_cfg_end - reg_cfg_start) - (dma_end - dma_start)) * 10 / 3);
#endif
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error);

    //uint32_t finished = calc_time_elapsed_us(started);

    //VIDEO_LOG("[%s:%d] all=%dus (copy_w:%d, copy_h:%d = %d)", __func__, __LINE__, finished, lvgl_cfg.copy_w, lvgl_cfg.copy_h, lvgl_cfg.copy_w * lvgl_cfg.copy_h);

    // CHECK_RET_EQ(ret, SUCCESS);
error:

    if(ret != CSK_DRIVER_OK)
    {
         dma2d_config_dump(&lvgl_cfg);
    }

    return ret;
}


static void dma2d_config_dump(lvgl_dma2d_cfg_t *pcfg)
{
    VIDEO_LOG("mode=%d", pcfg->mode);
    VIDEO_LOG("buf=0x%x", pcfg->buf);
    VIDEO_LOG("map=0x%x", pcfg->map);
    VIDEO_LOG("mask=0x%x", pcfg->mask);
    VIDEO_LOG("buf_w=%d", pcfg->buf_w);
    VIDEO_LOG("map_w=%d", pcfg->map_w);
    VIDEO_LOG("mask_w=%d", pcfg->mask_w);
    VIDEO_LOG("copy_w=%d", pcfg->copy_w);
    VIDEO_LOG("copy_h=%d", pcfg->copy_h);
    VIDEO_LOG("color=0x%x", pcfg->color);
    VIDEO_LOG("opa=0x%x", pcfg->opa);
}


void lvgl_gpdma_reg_dump(void)
{
    VIDEO_LOG("000 DMA_CH0_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH0_CTRL                      .all);   
    VIDEO_LOG("004 DMA_CH1_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH1_CTRL                      .all);   
    VIDEO_LOG("008 DMA_CH2_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH2_CTRL                      .all);   
    VIDEO_LOG("00C DMA_CH3_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH3_CTRL                      .all);   
    VIDEO_LOG("010 DMA_CH4_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH4_CTRL                      .all);   
    VIDEO_LOG("014 DMA_CH5_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH5_CTRL                      .all);   
    VIDEO_LOG("018 DMA_CH6_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH6_CTRL                      .all);   
    VIDEO_LOG("01C DMA_CH7_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH7_CTRL                      .all);   
    VIDEO_LOG("020 DMA_CH8_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH8_CTRL                      .all);   
    VIDEO_LOG("024 DMA_CH9_CTRL                  0x%08x", IP_GPDMA->REG_DMA_CH9_CTRL                      .all);   
    VIDEO_LOG("028 DMA_INT_EN                    0x%08x", IP_GPDMA->REG_DMA_INT_EN                        .all);   
    VIDEO_LOG("02C DMA_BLOCK_LEN_CH0             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH0                 .all);   
    VIDEO_LOG("030 DMA_BLOCK_LEN_CH1             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH1                 .all);   
    VIDEO_LOG("034 DMA_BLOCK_LEN_CH2             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH2                 .all);   
    VIDEO_LOG("038 DMA_BLOCK_LEN_CH3             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH3                 .all);   
    VIDEO_LOG("03C DMA_BLOCK_LEN_CH4             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH4                 .all);   
    VIDEO_LOG("040 DMA_BLOCK_LEN_CH5             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH5                 .all);   
    VIDEO_LOG("044 DMA_BLOCK_LEN_CH6             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH6                 .all);   
    VIDEO_LOG("048 DMA_BLOCK_LEN_CH7             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH7                 .all);   
    VIDEO_LOG("04C DMA_BLOCK_LEN_CH8             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH8                 .all);   
    VIDEO_LOG("050 DMA_BLOCK_LEN_CH9             0x%08x", IP_GPDMA->REG_DMA_BLOCK_LEN_CH9                 .all);   
    VIDEO_LOG("054 DMA_SRC_ADDR0_CH0             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH0                 .all);   
    VIDEO_LOG("058 DMA_SRC_ADDR1_CH0             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH0                 .all);   
    VIDEO_LOG("05C DMA_DST_ADDR0_CH0             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH0                 .all);   
    VIDEO_LOG("060 DMA_DST_ADDR1_CH0             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH0                 .all);   
    VIDEO_LOG("064 DMA_SRC_ADDR0_CH1             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH1                 .all);   
    VIDEO_LOG("068 DMA_SRC_ADDR1_CH1             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH1                 .all);   
    VIDEO_LOG("06C DMA_DST_ADDR0_CH1             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH1                 .all);   
    VIDEO_LOG("070 DMA_DST_ADDR1_CH1             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH1                 .all);   
    VIDEO_LOG("074 DMA_SRC_ADDR0_CH2             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH2                 .all);   
    VIDEO_LOG("078 DMA_SRC_ADDR1_CH2             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH2                 .all);   
    VIDEO_LOG("07C DMA_DST_ADDR0_CH2             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH2                 .all);   
    VIDEO_LOG("080 DMA_DST_ADDR1_CH2             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH2                 .all);   
    VIDEO_LOG("084 DMA_SRC_ADDR0_CH3             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH3                 .all);   
    VIDEO_LOG("088 DMA_SRC_ADDR1_CH3             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH3                 .all);   
    VIDEO_LOG("08C DMA_DST_ADDR0_CH3             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH3                 .all);   
    VIDEO_LOG("090 DMA_DST_ADDR1_CH3             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH3                 .all);   
    VIDEO_LOG("094 DMA_SRC_ADDR0_CH4             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH4                 .all);   
    VIDEO_LOG("098 DMA_SRC_ADDR1_CH4             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH4                 .all);   
    VIDEO_LOG("09C DMA_DST_ADDR0_CH4             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH4                 .all);   
    VIDEO_LOG("100 DMA_DST_ADDR1_CH4             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH4                 .all);   
    VIDEO_LOG("104 DMA_SRC_ADDR0_CH5             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH5                 .all);   
    VIDEO_LOG("108 DMA_SRC_ADDR1_CH5             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH5                 .all);   
    VIDEO_LOG("10C DMA_DST_ADDR0_CH5             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH5                 .all);   
    VIDEO_LOG("110 DMA_DST_ADDR1_CH5             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH5                 .all);   
    VIDEO_LOG("114 DMA_SRC_ADDR0_CH6             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH6                 .all);   
    VIDEO_LOG("118 DMA_SRC_ADDR1_CH6             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH6                 .all);   
    VIDEO_LOG("11C DMA_DST_ADDR0_CH6             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH6                 .all);   
    VIDEO_LOG("120 DMA_DST_ADDR1_CH6             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH6                 .all);   
    VIDEO_LOG("124 DMA_SRC_ADDR0_CH7             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH7                 .all);   
    VIDEO_LOG("128 DMA_SRC_ADDR1_CH7             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH7                 .all);   
    VIDEO_LOG("12C DMA_DST_ADDR0_CH7             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH7                 .all);   
    VIDEO_LOG("130 DMA_DST_ADDR1_CH7             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH7                 .all);   
    VIDEO_LOG("134 DMA_SRC_ADDR0_CH8             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH8                 .all);   
    VIDEO_LOG("138 DMA_SRC_ADDR1_CH8             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH8                 .all);   
    VIDEO_LOG("13C DMA_DST_ADDR0_CH8             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH8                 .all);   
    VIDEO_LOG("140 DMA_DST_ADDR1_CH8             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH8                 .all);   
    VIDEO_LOG("144 DMA_SRC_ADDR0_CH9             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR0_CH9                 .all);   
    VIDEO_LOG("148 DMA_SRC_ADDR1_CH9             0x%08x", IP_GPDMA->REG_DMA_SRC_ADDR1_CH9                 .all);   
    VIDEO_LOG("14C DMA_DST_ADDR0_CH9             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR0_CH9                 .all);   
    VIDEO_LOG("150 DMA_DST_ADDR1_CH9             0x%08x", IP_GPDMA->REG_DMA_DST_ADDR1_CH9                 .all);   
    VIDEO_LOG("154 DMA_INT_CLR                   0x%08x", IP_GPDMA->REG_DMA_INT_CLR                       .all);   
    VIDEO_LOG("158 DMA_INT_STATUS                0x%08x", IP_GPDMA->REG_DMA_INT_STATUS                    .all);   
    VIDEO_LOG("15C DMA_ERROR_INT_MASK            0x%08x", IP_GPDMA->REG_DMA_ERROR_INT_MASK                .all);   
    VIDEO_LOG("160 DMA_ERROR_INT_EN              0x%08x", IP_GPDMA->REG_DMA_ERROR_INT_EN                  .all);   
    VIDEO_LOG("164 DMA_IMAGE_PROC_BYPASS0        0x%08x", IP_GPDMA->REG_DMA_IMAGE_PROC_BYPASS0            .all);   
    VIDEO_LOG("168 DMA_IMAGE_PROC_BYPASS1        0x%08x", IP_GPDMA->REG_DMA_IMAGE_PROC_BYPASS1            .all);   
    VIDEO_LOG("16C DMA_RGB_MODE                  0x%08x", IP_GPDMA->REG_DMA_RGB_MODE                      .all);   
    VIDEO_LOG("18C DMA_IMAGE_SIZE_CONFIG_IN_CH6  0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6      .all);   
    VIDEO_LOG("190 DMA_IMAGE_SIZE_CONFIG_OUT_CH6 0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6     .all);   
    VIDEO_LOG("194 DMA_IMAGE_SIZE_CONFIG_IN_CH7  0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7      .all);   
    VIDEO_LOG("198 DMA_IMAGE_SIZE_CONFIG_OUT_CH7 0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7     .all);   
    VIDEO_LOG("19C DMA_IMAGE_SIZE_CONFIG_IN_CH8  0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8      .all);   
    VIDEO_LOG("1A0 DMA_IMAGE_SIZE_CONFIG_OUT_CH8 0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8     .all);   
    VIDEO_LOG("1A4 DMA_IMAGE_SIZE_CONFIG_IN_CH9  0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9      .all);   
    VIDEO_LOG("1A8 DMA_IMAGE_SIZE_CONFIG_OUT_CH9 0x%08x", IP_GPDMA->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9     .all);   
    VIDEO_LOG("1AC DMA_IMAGE_FORMAT              0x%08x", IP_GPDMA->REG_DMA_IMAGE_FORMAT                  .all);   
    VIDEO_LOG("1B0 DMA_IMAGE_ENC_DEC_CTRL        0x%08x", IP_GPDMA->REG_DMA_IMAGE_ENC_DEC_CTRL            .all);   
    VIDEO_LOG("1B4 DMA_CH_CLR                    0x%08x", IP_GPDMA->REG_DMA_CH_CLR                        .all);   
    VIDEO_LOG("1B8 DMA_ENC_OUT2D_BYPASS          0x%08x", IP_GPDMA->REG_DMA_ENC_OUT2D_BYPASS              .all);   
    VIDEO_LOG("1BC DMA_DEC_OUT2D_BYPASS          0x%08x", IP_GPDMA->REG_DMA_DEC_OUT2D_BYPASS              .all);   
    VIDEO_LOG("1C0 DMA_ENC_IN2D_BYPASS           0x%08x", IP_GPDMA->REG_DMA_ENC_IN2D_BYPASS               .all);   
    VIDEO_LOG("1C4 DMA_DEC_IN2D_BYPASS           0x%08x", IP_GPDMA->REG_DMA_DEC_IN2D_BYPASS               .all);   
    VIDEO_LOG("1D8 DMA_ZOOM_CTRL_CH6             0x%08x", IP_GPDMA->REG_DMA_ZOOM_CTRL_CH6                 .all);   
    VIDEO_LOG("1DC DMA_ZOOM_CTRL_CH7             0x%08x", IP_GPDMA->REG_DMA_ZOOM_CTRL_CH7                 .all);   
    VIDEO_LOG("1E0 DMA_ZOOM_CTRL_CH8             0x%08x", IP_GPDMA->REG_DMA_ZOOM_CTRL_CH8                 .all);   
    VIDEO_LOG("1E4 DMA_ZOOM_CTRL_CH9             0x%08x", IP_GPDMA->REG_DMA_ZOOM_CTRL_CH9                 .all);   
    VIDEO_LOG("1E8 DMA_IMAGE_OUT_BLOCK_LEN_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6       .all);   
    VIDEO_LOG("1EC DMA_IMAGE_OUT_BLOCK_LEN_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7       .all);   
    VIDEO_LOG("1F0 DMA_ZOOM_MODE                 0x%08x", IP_GPDMA->REG_DMA_ZOOM_MODE                     .all);   
    VIDEO_LOG("1F4 DMA_CH_TRIGGER_CTRL           0x%08x", IP_GPDMA->REG_DMA_CH_TRIGGER_CTRL               .all);   
    VIDEO_LOG("1F8 DMA_DIAG_SEL                  0x%08x", IP_GPDMA->REG_DMA_DIAG_SEL                      .all);   
    VIDEO_LOG("1FC DMA_DIAG_RPT                  0x%08x", IP_GPDMA->REG_DMA_DIAG_RPT                      .all);   
    VIDEO_LOG("200 DMA_SCATTER_GATHER_CTRL_CH0   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH0       .all);   
    VIDEO_LOG("204 DMA_SCATTER_GATHER_CTRL_CH1   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH1       .all);   
    VIDEO_LOG("208 DMA_SCATTER_GATHER_CTRL_CH2   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH2       .all);   
    VIDEO_LOG("20C DMA_SCATTER_GATHER_CTRL_CH3   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH3       .all);   
    VIDEO_LOG("210 DMA_SCATTER_GATHER_CTRL_CH4   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH4       .all);   
    VIDEO_LOG("214 DMA_SCATTER_GATHER_CTRL_CH5   0x%08x", IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH5       .all);   
    VIDEO_LOG("218 DMA_IMAGE_OUT_BLOCK_LEN_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8       .all);   
    VIDEO_LOG("21C DMA_IMAGE_OUT_BLOCK_LEN_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9       .all);   
    VIDEO_LOG("220 DMA_IMAGE_D2_ADDR_CTRL0_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6       .all);   
    VIDEO_LOG("224 DMA_IMAGE_D2_ADDR_CTRL1_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6       .all);   
    VIDEO_LOG("228 DMA_IMAGE_D2_ADDR_CTRL2_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH6       .all);   
    VIDEO_LOG("22C DMA_IMAGE_D2_ADDR_CTRL3_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6       .all);   
    VIDEO_LOG("230 DMA_IMAGE_D2_ADDR_CTRL4_CH6   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6       .all);   
    VIDEO_LOG("234 DMA_IMAGE_D2_ADDR_CTRL0_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7       .all);   
    VIDEO_LOG("238 DMA_IMAGE_D2_ADDR_CTRL1_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7       .all);   
    VIDEO_LOG("23C DMA_IMAGE_D2_ADDR_CTRL2_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH7       .all);   
    VIDEO_LOG("240 DMA_IMAGE_D2_ADDR_CTRL3_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7       .all);   
    VIDEO_LOG("244 DMA_IMAGE_D2_ADDR_CTRL4_CH7   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7       .all);   
    VIDEO_LOG("248 DMA_IMAGE_D2_ADDR_CTRL0_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8       .all);   
    VIDEO_LOG("24C DMA_IMAGE_D2_ADDR_CTRL1_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8       .all);   
    VIDEO_LOG("250 DMA_IMAGE_D2_ADDR_CTRL2_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH8       .all);   
    VIDEO_LOG("254 DMA_IMAGE_D2_ADDR_CTRL3_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8       .all);   
    VIDEO_LOG("258 DMA_IMAGE_D2_ADDR_CTRL4_CH8   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8       .all);   
    VIDEO_LOG("25C DMA_IMAGE_D2_ADDR_CTRL0_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9       .all);   
    VIDEO_LOG("260 DMA_IMAGE_D2_ADDR_CTRL1_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9       .all);   
    VIDEO_LOG("264 DMA_IMAGE_D2_ADDR_CTRL2_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL2_CH9       .all);   
    VIDEO_LOG("268 DMA_IMAGE_D2_ADDR_CTRL3_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9       .all);   
    VIDEO_LOG("26C DMA_IMAGE_D2_ADDR_CTRL4_CH9   0x%08x", IP_GPDMA->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9       .all);   
    VIDEO_LOG("270 DMA_DST_TRANS_BASE_UNIT       0x%08x", IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT           .all);   
    VIDEO_LOG("274 DMA_PO_BLOCK_LEN_CH_00        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00            .all);   
    VIDEO_LOG("278 DMA_PO_BLOCK_LEN_CH_01        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_01            .all);   
    VIDEO_LOG("27C DMA_PO_BLOCK_LEN_CH_02        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_02            .all);   
    VIDEO_LOG("280 DMA_PO_BLOCK_LEN_CH_03        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_03            .all);   
    VIDEO_LOG("284 DMA_PO_BLOCK_LEN_CH_04        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_04            .all);   
    VIDEO_LOG("288 DMA_PO_BLOCK_LEN_CH_05        0x%08x", IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_05            .all);   
    VIDEO_LOG("28C DMA_IMAGE_UV_STEP_S_67        0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_S_67            .all);   
    VIDEO_LOG("290 DMA_IMAGE_UV_STEP_S_89        0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_S_89            .all);   
    VIDEO_LOG("294 DMA_IMAGE_UV_STEP_L0_CH6      0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_L0_CH6          .all);   
    VIDEO_LOG("298 DMA_IMAGE_UV_STEP_L0_CH7      0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_L0_CH7          .all);   
    VIDEO_LOG("29C DMA_IMAGE_UV_STEP_L0_CH8      0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_L0_CH8          .all);   
    VIDEO_LOG("2A0 DMA_IMAGE_UV_STEP_L0_CH9      0x%08x", IP_GPDMA->REG_DMA_IMAGE_UV_STEP_L0_CH9          .all);   
    VIDEO_LOG("2A4 DMA_IMAGE_INT_EN              0x%08x", IP_GPDMA->REG_DMA_IMAGE_INT_EN                  .all);   
    VIDEO_LOG("2A8 DMA_IMAGE_INT_CLR             0x%08x", IP_GPDMA->REG_DMA_IMAGE_INT_CLR                 .all);   
    VIDEO_LOG("2AC DMA_IMAGE_INT_STATUS          0x%08x", IP_GPDMA->REG_DMA_IMAGE_INT_STATUS              .all);   
    VIDEO_LOG("2B0 DMA_IMAGE_FEATURE_CTRL        0x%08x", IP_GPDMA->REG_DMA_IMAGE_FEATURE_CTRL            .all);   
    VIDEO_LOG("2B4 DMA_IMAGE_FEATURE_CTRL0       0x%08x", IP_GPDMA->REG_DMA_IMAGE_FEATURE_CTRL0           .all);   
}


void lvgl_blender_reg_dump(void)
{
    VIDEO_LOG("0x00 IMAGE_PROC_EN   0x%08x", IP_D2BLENDER->REG_BLENDER_EN.all);
    VIDEO_LOG("0x04 BLENDER_CTRL    0x%08x", IP_D2BLENDER->REG_BLENDER_CTRL.all);
    VIDEO_LOG("0x08 ALPHA           0x%08x", IP_D2BLENDER->REG_ALPHA.all);
    VIDEO_LOG("0x0C COLOR           0x%08x", IP_D2BLENDER->REG_COLOR.all);
    VIDEO_LOG("0x10 FIFO_BURST_THD  0x%08x", IP_D2BLENDER->REG_FIFO_BURST_THD.all);
    VIDEO_LOG("0x14 FORE_SIZE       0x%08x", IP_D2BLENDER->REG_D2BLENDER_FORE_SIZE.all);
    VIDEO_LOG("0x18 BACK_SIZE       0x%08x", IP_D2BLENDER->REG_D2BLENDER_BACK_SIZE.all);
    VIDEO_LOG("0x1C MASK_SIZE       0x%08x", IP_D2BLENDER->REG_D2BLENDER_MASK_SIZE.all);
}



