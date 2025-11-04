/*
 * gp_dma.c
 *
 *  Created on: Sep 22, 2023
 *      Author: USER
 */
#include "Driver_GPDMA.h"
#include "arcs_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include <string.h> // for memset etc.

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(a)  (sizeof(a)/sizeof(a[0]))
#endif

// GPDMA Control register
#define GPDMA_CH_CTRL_HS_SEL_OFFSET         28
#define GPDMA_CH_CTRL_CFG_SGEN_OFFSET       27
#define GPDMA_CH_CTRL_CFG_DSEN_OFFSET       26
#define GPDMA_CH_CTRL_CH_PRIO_OFFSET        24
#define GPDMA_CH_CTRL_PRE_FETCH_OFFSET      23
#define GPDMA_CH_CTRL_FORCE_ON_OFFSET       22
#define GPDMA_CH_CTRL_HALF_BLOCK_MASK_OFFSET    21
#define GPDMA_CH_CTRL_BLOCK_MASK_OFFSET         20
#define GPDMA_CH_CTRL_READ_DONE_ACK_OFFSET  19
#define GPDMA_CH_CTRL_FLOW_CTRL_OFFSET      18
#define GPDMA_CH_CTRL_DST_BURST_OFFSET      16
#define GPDMA_CH_CTRL_SRC_BURST_OFFSET      14
#define GPDMA_CH_CTRL_AUTO_TFR_OFFSET       13
#define GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET  12
#define GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET  11
#define GPDMA_CH_CTRL_DST_INC_MODE_OFFSET   10
#define GPDMA_CH_CTRL_SRC_INC_MODE_OFFSET   9
#define GPDMA_CH_CTRL_SAMPLE_UNIT_OFFSET    6
#define GPDMA_CH_CTRL_TFR_MODE_OFFSET       4
#define GPDMA_CH_CTRL_STOP_MODE_OFFSET      3
#define GPDMA_CH_CTRL_STOP_OFFSET           2
#define GPDMA_CH_CTRL_CH_START_OFFSET       1
#define GPDMA_CH_CTRL_CH_EN_OFFSET          0

#define GPDMA_CH_CTRL_HALF_BLOCK_MASK_BIT   (0x1 << 21) // half block finish interrupt mask bit
#define GPDMA_CH_CTRL_BLOCK_MASK_BIT        (0x1 << 20) // block finish interrupt mask bit
#define GPDMA_CH_CTRL_AUTO_TFR_BIT          (0x1 << 13) // continuous transmission mode
#define GPDMA_CH_CTRL_DST_PIPO_MODE_BIT     (0x1 << 12) // set Ping/Pong for destination address
#define GPDMA_CH_CTRL_SRC_PIPO_MODE_BIT     (0x1 << 11) // set Ping/Pong for source address
#define GPDMA_CH_CTRL_IMMEDIATE_STOP_BIT    (0x1 << 3)  // 1: immediate stop, 0: stop when current block done
#define GPDMA_CH_CTRL_STOP_BIT              (0x1 << 2)  // [W1p] stop channel transfer bit
#define GPDMA_CH_CTRL_START_BIT             (0x1 << 1)  // [W1P] start channel transfer bit
#define GPDMA_CH_CTRL_CH_EN_BIT             (0x1 << 0)  // channel enable bit

// GPDMA Scatter and Gather reigster
#define GPDMA_CH_GATHER_COUNTER_OFFSET      26
#define GPDMA_CH_GATHER_INTERVAL_OFFSET     16
#define GPDMA_CH_SCATTER_COUNTER_OFFSET     10
#define GPDMA_CH_SCATTER_INTERVAL_OFFSET    0

#define GPDMA_CH_GATHER_COUNTER_MASK        0x3F
#define GPDMA_CH_GATHER_INTERVAL_MASK       0x3FF
#define GPDMA_CH_SCATTER_COUNTER_MASK       0x3F
#define GPDMA_CH_SCATTER_INTERVAL_MASK      0x3FF

// GPDMA DIAGRAM CONFIGURE
#define GPDMA_DIAG_SEL_TOG_FLAG_MODE        0
#define GPDMA_DIAG_SEL_REMAIN_CNT_MODE      0
#define GPDMA_DIAG_SEL_SRC_ADDRESS_MODE     4
#define GPDMA_DIAG_SEL_DST_ADDRESS_MODE     5

// GPDMA MAX BLOCK
#if GPDMAC_ARCS_D0
#define GPDMA_CH_MAX_BLOCK_LENGTH           0xFFFFF //20bit
#define GPDMA_CH_MAX_BLOCK_LENGTH_MASK      0xFFFFF
#define GPDMA_CH_MAX_BLOCK_LENGTH_BITNUM    20
#else //ARCS_C0
#define GPDMA_CH_MAX_BLOCK_LENGTH           0xFFFF //16bit
#define GPDMA_CH_MAX_BLOCK_LENGTH_MASK      0xFFFF
#define GPDMA_CH_MAX_BLOCK_LENGTH_BITNUM    16
#endif // GPDMAC_ARCS_D0

#define GPDMA_DIAG_RPT_SRC_ADDR_POS         (GPDMA_CH_MAX_BLOCK_LENGTH_BITNUM + 0)
#define GPDMA_DIAG_RPT_DST_ADDR_POS         (GPDMA_CH_MAX_BLOCK_LENGTH_BITNUM + 1)
#define GPDMA_DIAG_RPT_CH_BUSY_POS          (GPDMA_CH_MAX_BLOCK_LENGTH_BITNUM + 2)

#define GPDMA_DIAG_RPT_SRC_ADDR_MASK        (0x1 << GPDMA_DIAG_RPT_SRC_ADDR_POS)
#define GPDMA_DIAG_RPT_DST_ADDR_MASK        (0x1 << GPDMA_DIAG_RPT_DST_ADDR_POS)
#define GPDMA_DIAG_RPT_CH_BUSY_MASK         (0x1 << GPDMA_DIAG_RPT_CH_BUSY_POS)
#define GPDMA_PING_PONG_MASK                (GPDMA_DIAG_RPT_SRC_ADDR_MASK | GPDMA_DIAG_RPT_DST_ADDR_MASK)


typedef struct _csk_gpdma_ch_info {
    CSK_GPDMA_SignalEvent_t cb_event;
    void* workspace;
    csk_gpdma_status_t status;
    // In normal mode: total length
    // In PiPo mode: (Ping) buffer length
    uint32_t length;
#if GPDMAC_ARCS_D0
    uint32_t length_po; // Pong buffer length
#endif
    // Current transfer length
    uint32_t xfer_length;
    // transfer mode
    uint32_t mode;
} csk_gpdma_ch_info_t;

static csk_gpdma_ch_info_t gpdma_ch_info[CSK_GPDMA_MAX_CHANNEL_NUM] = {0};

static volatile uint8_t GPDMA_GLB_FLAG = 0;

static void GPDMA_IRQ_Handler(void);

int32_t
GPDMA_Initialize(void){

    if (GPDMA_GLB_FLAG == 0){

#if CONFIG_ARCS_GPDMA_DATA_ONLY
        memset(gpdma_ch_info, 0, sizeof(gpdma_ch_info));
        register_ISR(IRQ_DMAC_GP_VECTOR, GPDMA_IRQ_Handler, NULL);
        enable_IRQ(IRQ_DMAC_GP_VECTOR);
        for (uint8_t i = 0; i < CSK_GPDMA_MAX_CHANNEL_NUM; i++){
            gpdma_ch_info[i].status = gpdma_status_init;
        }
        return 0;
#endif
        // Rest GPDMA module
        IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 1;

        memset(gpdma_ch_info, 0, sizeof(gpdma_ch_info));

	#if GPDMAC_ARCS_D0
        __HAL_CRM_GPDMA_CLK_ENABLE();

        // Clean GPDMA interrupt
        IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_BLOCK_FINISH_CLR = 0x3F;
        IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_HALF_BLOCK_FINISH_CLR = 0x3F;

        // Clean GPDMA error
        IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_DMA_AHB_ERR_CLR = 0x1;

        IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x3F;
	#else
        __HAL_CRM_DMA_GP_CLK_ENABLE();

        // Clean GPDMA interrupt
        IP_GPDMA->REG_DMA_BLOCK_FINISH_CLR.bit.CFG_BLOCK_FINISH_CLR = 0x3FF;
        IP_GPDMA->REG_DMA_BLOCK_FINISH_CLR.bit.CFG_HALF_BLOCK_FINISH_CLR = 0x3FF;

        // Clean GPDMA error
        IP_GPDMA->REG_DMA_AHB_ERROR_CLR.bit.CFG_DMA_AHB_ERR_CLR = 0x1;

        IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x3FF;
	#endif

        // Disable GPDMA interrupt
        IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 0x0;
        IP_GPDMA->REG_DMA_INT_EN.bit.CFG_HALF_BLOCK_FINISH_INT_EN = 0x0;

        // Register GPDMA global interrupt
        register_ISR(IRQ_DMAC_GP_VECTOR, GPDMA_IRQ_Handler, NULL);
        enable_IRQ(IRQ_DMAC_GP_VECTOR);

        uint8_t i = 0;
        for (i = 0; i < CSK_GPDMA_MAX_CHANNEL_NUM; i++){
            gpdma_ch_info[i].status = gpdma_status_init;
        }
    }

    GPDMA_GLB_FLAG++;

    return CSK_DRIVER_OK;
}

int32_t
GPDMA_Uninitialize(void){
    GPDMA_GLB_FLAG--;

    if (GPDMA_GLB_FLAG == 0){
        // Device reset
        IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 1;

        // Disable GPDMA interrupt
        IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 0x0;
        IP_GPDMA->REG_DMA_INT_EN.bit.CFG_HALF_BLOCK_FINISH_INT_EN = 0x0;

        // Disable interrupt
        disable_IRQ(IRQ_DMAC_GP_VECTOR);
        // Unregister interrupt
        register_ISR(IRQ_DMAC_GP_VECTOR, NULL, NULL);

        // clear information
        memset(gpdma_ch_info, 0, sizeof(gpdma_ch_info));

	#if GPDMAC_ARCS_D0
        __HAL_CRM_GPDMA_CLK_DISABLE();
	#else
        __HAL_CRM_DMA_GP_CLK_DISABLE();
	#endif

        // clear channel configure
        IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x3FF;

        uint8_t i = 0;
        for (i = 0; i < CSK_GPDMA_MAX_CHANNEL_NUM; i++){
            gpdma_ch_info[i].status = gpdma_status_none;
        }
    }

    return CSK_DRIVER_OK;
}

int32_t
GPDMA_Config(csk_gpdma_init_t* res, CSK_GPDMA_SignalEvent_t cb_event, void* workspace){
    if ((gpdma_ch_info[res->dma_ch].status == gpdma_status_none) || (gpdma_ch_info[res->dma_ch].status == gpdma_status_busy)){
        return CSK_GPDMA_STATUS_ERROR;
    }

    gpdma_ch_info[res->dma_ch].cb_event = cb_event;
    gpdma_ch_info[res->dma_ch].workspace = workspace;

    uint8_t channel = (uint8_t)res->dma_ch;
    if (channel >= CSK_GPDMA_MAX_CHANNEL_NUM){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Scatter gather parameter error
    if (res->gather_counter == 0 && res->gather_en == 1){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (res->scatter_counter == 0 && res->scatter_en == 1){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Get channel control base address
    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
    ch_ctrl += channel;

    {
        uint32_t ch_ctrl_para = 0;

        // channel prio configure
        {
            ch_ctrl_para |= (res->prio_lvl << GPDMA_CH_CTRL_CH_PRIO_OFFSET);
        }

        // burst length config
        {
            ch_ctrl_para |= (res->burst_len << GPDMA_CH_CTRL_SRC_BURST_OFFSET | res->burst_len << GPDMA_CH_CTRL_DST_BURST_OFFSET);
        }

        // normal mode or PIPO mode
        {
            uint32_t src_mode, dst_mode;

            src_mode = res->src_mode;
            dst_mode = res->dst_mode;

            // If one mode in PIPO mode, need configure auto transfer
            if (src_mode || dst_mode){
                ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_AUTO_TFR_OFFSET);
            }

            ch_ctrl_para |= (src_mode << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET | dst_mode << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET);
        }

        // address mode configure
        {
            uint32_t src_add_mode, dst_add_mode;

            src_add_mode = res->src_inc_mode;
            dst_add_mode = res->dst_inc_mode;

            ch_ctrl_para |= (src_add_mode << GPDMA_CH_CTRL_SRC_INC_MODE_OFFSET | dst_add_mode << GPDMA_CH_CTRL_DST_INC_MODE_OFFSET);
        }

        // transfer mode
        {
            ch_ctrl_para |= (res->tfr_mode << GPDMA_CH_CTRL_TFR_MODE_OFFSET);

            // configure handshake
            if (res->tfr_mode != tfr_mode_m2m){
                // Clear flow control

                if (res->handshake == hs_none){
                    return CSK_DRIVER_ERROR_PARAMETER;
                }

                if (res->handshake >= hs_max){
                    return CSK_DRIVER_ERROR_PARAMETER;
                }

#if !GPDMAC_ARCS_D0
                // DMA SELECT 1
                if ((res->handshake & 0x10) == 0x10){
                    IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x1;
                }

                // DMA SELECT 2
                else if ((res->handshake & 0x20) == 0x20){
                    IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x2;
                }

                // DMA SELECT 0
                else {
                    IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x0;
                }
#endif

                ch_ctrl_para |= (res->handshake & 0xF) << GPDMA_CH_CTRL_HS_SEL_OFFSET;
            } else {
                // Flow control
                ch_ctrl_para |= (0x0 << GPDMA_CH_CTRL_FLOW_CTRL_OFFSET);
            }
        }
#if GPDMAC_ARCS_D0
        // Sample unit
        {
            // Configure source unit is same with destination unit
            ch_ctrl_para |= (res->sample_unit  << GPDMA_CH_CTRL_SAMPLE_UNIT_OFFSET);
            IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.all &= ~(0x3 << channel*2);
            IP_GPDMA->REG_DMA_DST_TRANS_BASE_UNIT.all |= (res->sample_unit << channel*2);
        }

        // Scatter Gather
        {
            uint32_t* scatter_gatter_base = (uint32_t*)&IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH0.all + channel;

            if (res->gather_en){
                ch_ctrl_para |= (res->gather_en << GPDMA_CH_CTRL_CFG_SGEN_OFFSET);

                *scatter_gatter_base &= ~((GPDMA_CH_GATHER_COUNTER_MASK << GPDMA_CH_GATHER_COUNTER_OFFSET) | (GPDMA_CH_GATHER_INTERVAL_MASK << GPDMA_CH_GATHER_INTERVAL_OFFSET));

                *scatter_gatter_base |= (res->gather_interval << GPDMA_CH_GATHER_INTERVAL_OFFSET) | ((res->gather_counter - 1) << GPDMA_CH_GATHER_COUNTER_OFFSET);
            }

            if (res->scatter_en){
                ch_ctrl_para |= (res->scatter_en << GPDMA_CH_CTRL_CFG_DSEN_OFFSET);

                *scatter_gatter_base &= ~((GPDMA_CH_SCATTER_COUNTER_MASK << GPDMA_CH_SCATTER_COUNTER_OFFSET) | (GPDMA_CH_SCATTER_INTERVAL_MASK << GPDMA_CH_SCATTER_INTERVAL_OFFSET));

                *scatter_gatter_base |= (res->scatter_interval << GPDMA_CH_SCATTER_INTERVAL_OFFSET) | ((res->scatter_counter - 1) << GPDMA_CH_SCATTER_COUNTER_OFFSET);
            }
        }
#endif // GPDMAC_ARCS_D0
        ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_CH_EN_OFFSET);

        *ch_ctrl = ch_ctrl_para;
    }

    gpdma_ch_info[res->dma_ch].status = gpdma_status_config;

    return CSK_DRIVER_OK;
}

#define PIPO_MODE_MASK \
    ((0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET)|(0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET))

int32_t
GPDMA_Config_Scatt_Gath(csk_gpdma_ch_t ch, csk_gpdma_scatt_gath_t* sg) { // Configure Scatter-Gather ONLY!

    if (ch >= CSK_GPDMA_MAX_CHANNEL_NUM){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((gpdma_ch_info[ch].status == gpdma_status_none) || (gpdma_ch_info[ch].status == gpdma_status_busy)){
        return CSK_GPDMA_STATUS_ERROR;
    }

/*
    // Scatter gather parameter error
    if (sg->gather_counter == 0 && sg->gather_en == 1){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (sg->scatter_counter == 0 && sg->scatter_en == 1){
        return CSK_DRIVER_ERROR_PARAMETER;
    }
*/

    // Get channel control base address
    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
    ch_ctrl += ch;
    uint32_t ch_ctrl_para = *ch_ctrl;
#if GPDMAC_ARCS_D0
    // Scatter Gather
    uint32_t* scatter_gatter_base = (uint32_t*)&IP_GPDMA->REG_DMA_SCATTER_GATHER_CTRL_CH0.all + ch;

    if (sg->gather_en){ // enable gather
        ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_CFG_SGEN_OFFSET);
        *scatter_gatter_base &= ~((GPDMA_CH_GATHER_COUNTER_MASK << GPDMA_CH_GATHER_COUNTER_OFFSET) | (GPDMA_CH_GATHER_INTERVAL_MASK << GPDMA_CH_GATHER_INTERVAL_OFFSET));
        *scatter_gatter_base |= (sg->gather_interval << GPDMA_CH_GATHER_INTERVAL_OFFSET) | ((sg->gather_counter - 1) << GPDMA_CH_GATHER_COUNTER_OFFSET);
    } else { // disable gather
        ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_CFG_SGEN_OFFSET);
    }

    if (sg->scatter_en){ // enable scatter
        ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_CFG_DSEN_OFFSET);
        *scatter_gatter_base &= ~((GPDMA_CH_SCATTER_COUNTER_MASK << GPDMA_CH_SCATTER_COUNTER_OFFSET) | (GPDMA_CH_SCATTER_INTERVAL_MASK << GPDMA_CH_SCATTER_INTERVAL_OFFSET));
        *scatter_gatter_base |= (sg->scatter_interval << GPDMA_CH_SCATTER_INTERVAL_OFFSET) | ((sg->scatter_counter - 1) << GPDMA_CH_SCATTER_COUNTER_OFFSET);
    } else { // disable scatter
        ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_CFG_DSEN_OFFSET);
    }
#endif // GPDMAC_ARCS_D0
    // src mode
    if (sg->src_mode != 0xFF) {
        if (sg->src_mode)
            ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET);
        else
            ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET);
    }

    // dst mode
    if (sg->dst_mode != 0xFF) {
        if (sg->dst_mode)
            ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET);
        else
            ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET);
    }

    // If one mode in PIPO mode, need configure auto transfer
    if (ch_ctrl_para & PIPO_MODE_MASK) {
        ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_AUTO_TFR_OFFSET);
    } else {
        ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_AUTO_TFR_OFFSET);
    }


    *ch_ctrl = ch_ctrl_para;
    return CSK_DRIVER_OK;
}

int32_t // src_mode / dst_mode = 0xff, don't change its original value...
GPDMA_Config_Addr_Mode(csk_gpdma_ch_t ch, csk_address_mode_t src_mode, csk_address_mode_t dst_mode) { // Configure address_mode ONLY!

    if (ch >= CSK_GPDMA_MAX_CHANNEL_NUM){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((gpdma_ch_info[ch].status == gpdma_status_none) || (gpdma_ch_info[ch].status == gpdma_status_busy)){
        return CSK_GPDMA_STATUS_ERROR;
    }

    // Get channel control base address
    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
    ch_ctrl += ch;
    uint32_t ch_ctrl_para = *ch_ctrl;

    // src mode
    if (src_mode != 0xFF) {
        if (src_mode == address_mode_pipo)
            ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET);
        else
            ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET);
    }

    // dst mode
    if (dst_mode != 0xFF) {
        if (dst_mode == address_mode_pipo)
            ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET);
        else
            ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET);
    }

    // If one mode in PIPO mode, need configure auto transfer
    if (ch_ctrl_para & PIPO_MODE_MASK) {
        ch_ctrl_para |= (0x1 << GPDMA_CH_CTRL_AUTO_TFR_OFFSET);
    } else {
        ch_ctrl_para &= ~(0x1 << GPDMA_CH_CTRL_AUTO_TFR_OFFSET);
    }

    *ch_ctrl = ch_ctrl_para;
    return CSK_DRIVER_OK;
}

int32_t // src_mode_p / dst_mode_p = NULL, don't care its address mode...
GPDMA_Get_Addr_Mode(csk_gpdma_ch_t ch, csk_address_mode_t *src_mode_p, csk_address_mode_t *dst_mode_p) {

    if (ch >= CSK_GPDMA_MAX_CHANNEL_NUM)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Get channel control base address
    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
    ch_ctrl += ch;
    uint32_t ch_ctrl_para = *ch_ctrl;

    if (src_mode_p != NULL)
        *src_mode_p = (csk_address_mode_t)((ch_ctrl_para >> GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET) & 0x1);

    if (dst_mode_p != NULL)
        *dst_mode_p = (csk_address_mode_t)((ch_ctrl_para >> GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET) & 0x1);

    return CSK_DRIVER_OK;
}


// return value: 0 indicates ping block, 1 indicates pong block
static uint8_t gpdma_ch_busy(csk_gpdma_ch_t ch)
{
    IP_GPDMA->REG_DMA_DIAG_SEL.all =
            (GPDMA_DIAG_SEL_TOG_FLAG_MODE << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_SEL_Pos) |
            (ch << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_CH_SEL_Pos);
    return (IP_GPDMA->REG_DMA_DIAG_RPT.all & GPDMA_DIAG_RPT_CH_BUSY_MASK) ? 1 : 0;
}

// return value: 0 indicates ping block, 1 indicates pong block
static uint8_t gpdma_get_pipo_sel(csk_gpdma_ch_t ch, uint8_t *ch_busy_p)
{
//    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_TOG_FLAG_MODE;
//    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = ch;

    IP_GPDMA->REG_DMA_DIAG_SEL.all =
            (GPDMA_DIAG_SEL_TOG_FLAG_MODE << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_SEL_Pos) |
            (ch << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_CH_SEL_Pos);
    uint32_t rpt_val = IP_GPDMA->REG_DMA_DIAG_RPT.all;
    if (ch_busy_p != NULL)
        *ch_busy_p = (rpt_val & GPDMA_DIAG_RPT_CH_BUSY_MASK) ? 1 : 0;
    return (rpt_val & GPDMA_PING_PONG_MASK) ? 1 : 0;
}

// *src_sel_p / *dst_sel_p: 0 indicates ping block, 1 indicates pong block
static inline void gpdma_get_pipo_sel2(csk_gpdma_ch_t ch, uint8_t *src_sel_p, uint8_t *dst_sel_p)
{
    IP_GPDMA->REG_DMA_DIAG_SEL.all =
            (GPDMA_DIAG_SEL_TOG_FLAG_MODE << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_SEL_Pos) |
            (ch << GP_DMAC_DMA_DIAG_SEL_CFG_DIAG_CH_SEL_Pos);
    uint32_t rpt_val = IP_GPDMA->REG_DMA_DIAG_RPT.all;
    if (src_sel_p != 0)
        *src_sel_p = (rpt_val & GPDMA_DIAG_RPT_SRC_ADDR_MASK) ? 1 : 0;
    if (dst_sel_p != 0)
        *dst_sel_p = (rpt_val & GPDMA_DIAG_RPT_DST_ADDR_MASK) ? 1 : 0;
}

#define GET_DMA_CH_ADDR(REG, ch, addr) \
        addr = (uint32_t*)&IP_GPDMA->REG_DMA_##REG##_CH0.all + ((ch) << 2); \
        if (addr >= &IP_GPDMA->REG_RESV_0XA0_0XFC[0])   \
            addr += ARRAY_COUNT(IP_GPDMA->REG_RESV_0XA0_0XFC);

#define SET_DMA_CH_ADDR(REG, ch, addr) { \
        uint32_t *addr_p = (uint32_t*)&IP_GPDMA->REG_DMA_##REG##_CH0.all + ((ch) << 2); \
        if (addr_p >= &IP_GPDMA->REG_RESV_0XA0_0XFC[0])   \
            addr_p += ARRAY_COUNT(IP_GPDMA->REG_RESV_0XA0_0XFC); \
        *addr_p = (uint32_t)addr; }

#define GET_DMA_CH_BLK_LEN(ch) \
        (*((uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all + (ch)))

#define SET_DMA_CH_BLK_LEN(ch, len) { \
        uint32_t *len_p = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all + (ch); \
        *len_p = (len); }

#define GET_DMA_CH_PO_BLK_LEN(ch) \
        (*((uint32_t*)&IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all + (ch)))

#define SET_DMA_CH_PO_BLK_LEN(ch, len) { \
        uint32_t *len_p = (uint32_t*)&IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all + (ch); \
        *len_p = (len); }

#define GET_DMA_CH_CTRL(ch) \
        (*((uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + (ch)))

#define SET_DMA_CH_CTRL(ch, ctrl_val) { \
        uint32_t *ctrl_p = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + (ch); \
        *ctrl_p = ctrl_val; }

int32_t
GPDMA_Start_Normal(csk_gpdma_ch_t ch, void* src, void* dst, uint32_t sample_len){
    uint8_t channel = ch;

    // Only in configure status can start normal transmits
    if (!(gpdma_ch_info[channel].status == gpdma_status_config)){
        return CSK_GPDMA_STATUS_ERROR;
    }

    // Get channel control register
//    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
//    ch_ctrl += channel;
    uint32_t ctrl_val = GET_DMA_CH_CTRL(channel);

    // source mode and destination mode must both be normal mode
//    if ((*ch_ctrl & (0x3 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET)) != 0x0){
    if ((ctrl_val & (0x3 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET)) != 0x0){
        return CSK_GPDMA_MODE_ERROR;
    }

/*
    uint32_t* src_address = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* dst_address = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all;

    // TODO This is a BUG for address fill error in EXCEL
    // Get channel source and destination register
    {
        if (channel >= gp_dma_ch5){
            // Get channel source and destination register
            src_address = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH5.all;
            dst_address = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH5.all;

            src_address += (channel - gp_dma_ch5) * 4;
            dst_address += (channel - gp_dma_ch5) * 4;
        } else {
            src_address += channel * 4;
            dst_address += channel * 4;
        }
    }

    // Configure source and destination
    *src_address = (uint32_t)src;
    *dst_address = (uint32_t)dst;
*/

    // Configure source and destination
    SET_DMA_CH_ADDR(SRC_ADDR0, channel, src);
    SET_DMA_CH_ADDR(DST_ADDR0, channel, dst);

    // Configure length
    gpdma_ch_info[channel].length = sample_len;
    gpdma_ch_info[channel].xfer_length = 0;

//    // Get channel length register
//    uint32_t* ch_len = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all;
//    ch_len += channel;
//
//    if (sample_len > GPDMA_CH_MAX_BLOCK_LENGTH){
//        *ch_len = GPDMA_CH_MAX_BLOCK_LENGTH;
//    } else {
//        *ch_len = sample_len;
//    }
    SET_DMA_CH_BLK_LEN(channel,
                    sample_len > GPDMA_CH_MAX_BLOCK_LENGTH ?
                    GPDMA_CH_MAX_BLOCK_LENGTH : sample_len);

    // Configure mode
    gpdma_ch_info[channel].mode = address_mode_normal;

    gpdma_ch_info[channel].status = gpdma_status_busy;

    // Enable finish interrupt
    IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 0x1;

    // Unmask block finish interrupt, and clear PIPO-related bits
    ctrl_val &= ~(GPDMA_CH_CTRL_BLOCK_MASK_BIT | GPDMA_CH_CTRL_SRC_PIPO_MODE_BIT |
                GPDMA_CH_CTRL_DST_PIPO_MODE_BIT | GPDMA_CH_CTRL_AUTO_TFR_BIT);

    // Enable
//    if (!(*ch_ctrl & 0x1)){
//        *ch_ctrl |= 0x1;
//    }

    // Start
//    *ch_ctrl |= 0x2;
    ctrl_val |= GPDMA_CH_CTRL_START_BIT | GPDMA_CH_CTRL_CH_EN_BIT;
    SET_DMA_CH_CTRL(channel, ctrl_val);

    return CSK_DRIVER_OK;
}

int32_t
#if GPDMAC_ARCS_D0
GPDMA_Start_PiPoEx(csk_gpdma_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t sample_len, uint32_t sample_len1)
#else
GPDMA_Start_PiPo(csk_gpdma_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t sample_len)
#endif
{
    uint8_t channel = ch;

    // Only in configure status can start pipo transmits
    if (gpdma_ch_info[channel].status != gpdma_status_config){
        return CSK_GPDMA_STATUS_ERROR;
    }

    // source0 and destination0 buffer must not be NULL pointer
    if (src0 == NULL || dst0 == NULL){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // source1 and destination1 buffer must be one of the PIPO mode
    if (src1 == NULL && dst1 == NULL){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

#if GPDMAC_ARCS_D0
    if (sample_len > GPDMA_CH_MAX_BLOCK_LENGTH || sample_len1 > GPDMA_CH_MAX_BLOCK_LENGTH){
#else
    if (sample_len > GPDMA_CH_MAX_BLOCK_LENGTH){
#endif
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Get channel control register
//    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
//    ch_ctrl += channel;
    uint32_t ctrl_val = GET_DMA_CH_CTRL(channel);

/*
    uint32_t* src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH0.all;
    uint32_t* dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all;
    uint32_t* dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH0.all;

    // TODO This is a BUG for address fill error in EXCEL
    // Get channel source and destination register
    {
        if (channel >= gp_dma_ch5){
            // Get channel source and destination register
            src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH5.all;
            src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH5.all;
            dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH5.all;
            dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH5.all;

            src_address0 += (channel - gp_dma_ch5) * 4;
            src_address1 += (channel - gp_dma_ch5) * 4;
            dst_address0 += (channel - gp_dma_ch5) * 4;
            dst_address1 += (channel - gp_dma_ch5) * 4;
        }

        else if (channel == gp_dma_ch4) {
            // Get channel source and destination register
            src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH4.all;
            src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH4.all;
            dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH4.all;
            dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH4.all;
        }

        else {
            src_address0 += channel * 4;
            src_address1 += channel * 4;
            dst_address0 += channel * 4;
            dst_address1 += channel * 4;
        }
    }
*/

//    uint32_t *src_address0, *src_address1, *dst_address0, *dst_address1;
//    GET_DMA_CH_ADDR(SRC_ADDR0, channel, src_address0);
//    GET_DMA_CH_ADDR(SRC_ADDR1, channel, src_address1);
//    GET_DMA_CH_ADDR(DST_ADDR0, channel, dst_address0);
//    GET_DMA_CH_ADDR(DST_ADDR1, channel, dst_address1);

    // Configure source and destination
//    *src_address0 = (uint32_t)src0;
//    *dst_address0 = (uint32_t)dst0;
    SET_DMA_CH_ADDR(SRC_ADDR0, channel, src0);
    SET_DMA_CH_ADDR(DST_ADDR0, channel, dst0);

    if (src1){
//        *src_address1 = (uint32_t)src1;
        SET_DMA_CH_ADDR(SRC_ADDR1, channel, src1);
    }

    if (dst1){
//        *dst_address1 = (uint32_t)dst1;
        SET_DMA_CH_ADDR(DST_ADDR1, channel, dst1);
    }

    // Configure length
//    uint32_t* ch_len = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all;
//    ch_len += channel;
//    *ch_len = sample_len;
    SET_DMA_CH_BLK_LEN(channel, sample_len);
    gpdma_ch_info[channel].xfer_length = 0;
    gpdma_ch_info[channel].length = sample_len;

#if GPDMAC_ARCS_D0
//    uint32_t* ch_len1 = (uint32_t*)&IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all;
//    ch_len1 += channel;
//    *ch_len1 = sample_len1;
    SET_DMA_CH_PO_BLK_LEN(channel, sample_len1);
    gpdma_ch_info[channel].length_po = sample_len1;
#endif

    // Configure mode
    gpdma_ch_info[channel].mode = address_mode_pipo;

    gpdma_ch_info[channel].status = gpdma_status_busy;

    // Enable finish interrupt
    IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 0x1;
    ctrl_val &= ~GPDMA_CH_CTRL_BLOCK_MASK_BIT;

    // Enable
//    if (!(*ch_ctrl & 0x1)){
//        *ch_ctrl |= 0x1;
//    }

    // Start & Set PIPO-related bits
    //*ch_ctrl |= 0x2;
    ctrl_val |= GPDMA_CH_CTRL_START_BIT | GPDMA_CH_CTRL_CH_EN_BIT | GPDMA_CH_CTRL_SRC_PIPO_MODE_BIT |
                GPDMA_CH_CTRL_DST_PIPO_MODE_BIT | GPDMA_CH_CTRL_AUTO_TFR_BIT;
    SET_DMA_CH_CTRL(channel, ctrl_val);

    return CSK_DRIVER_OK;
}

int32_t
#if GPDMAC_ARCS_D0
//NOTE: sample_len = 0 indicates the original length is kept unchanged.
GPDMA_PiPo_ReloadEx(csk_gpdma_ch_t ch, void* src, void* dst, uint32_t sample_len, uint32_t flags)
#else
GPDMA_PiPo_Reload(csk_gpdma_ch_t ch, void* src, void* dst)
#endif
{
    uint8_t channel = ch;

    if (gpdma_ch_info[channel].status != gpdma_status_busy){
        return CSK_GPDMA_STATUS_ERROR;
    }

    if (gpdma_ch_info[channel].mode != address_mode_pipo){
        return CSK_GPDMA_MODE_ERROR;
    }

    if ((src == NULL) && (dst == NULL)){
        return CSK_DRIVER_ERROR_PARAMETER;
    }

//    uint32_t* channel_control = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + channel;
    uint32_t ctrl_val = GET_DMA_CH_CTRL(channel);

/*
    uint32_t* src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH0.all;
    uint32_t* dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all;
    uint32_t* dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH0.all;

    // TODO This is a BUG for address fill error in EXCEL
    // Get channel source and destination register
    {
        if (channel >= gp_dma_ch5){
            // Get channel source and destination register
            src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH5.all;
            src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH5.all;
            dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH5.all;
            dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH5.all;

            src_address0 += (channel - gp_dma_ch5) * 4;
            src_address1 += (channel - gp_dma_ch5) * 4;
            dst_address0 += (channel - gp_dma_ch5) * 4;
            dst_address1 += (channel - gp_dma_ch5) * 4;
        }

        else if (channel == gp_dma_ch4) {
            // Get channel source and destination register
            src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH4.all;
            src_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR1_CH4.all;
            dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH4.all;
            dst_address1 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR1_CH4.all;
        }

        else {
            src_address0 += channel * 4;
            src_address1 += channel * 4;
            dst_address0 += channel * 4;
            dst_address1 += channel * 4;
        }
    }

    // Choose remain length register
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_TOG_FLAG_MODE;
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

    uint8_t src_tog_flg = (IP_GPDMA->REG_DMA_DIAG_RPT.all & 0x100000) >> 20;
    uint8_t dst_tog_flg = (IP_GPDMA->REG_DMA_DIAG_RPT.all & 0x200000) >> 21;

    // Set channel source address which configure pipo mode
    if ((src != NULL) && (*channel_control & 0x1 << GPDMA_CH_CTRL_SRC_PIPO_MODE_OFFSET)){
        // Current address0 is busy
        if (src_tog_flg == 0){
            *src_address0 = (uint32_t)src;
        } else { // Current address1 is busy
            *src_address1 = (uint32_t)src;
        }
    }

    if ((dst != NULL) && (*channel_control & 0x1 << GPDMA_CH_CTRL_DST_PIPO_MODE_OFFSET)){
        // Current address0 is busy
        if (dst_tog_flg == 0){
            *dst_address0 = (uint32_t)dst;
        } else { // Current address1 is busy
            *dst_address1 = (uint32_t)dst;
        }
    }
*/

    // Check ping/pong selection for source & destination address
    uint8_t src_tog_flg, dst_tog_flg;
    gpdma_get_pipo_sel2(ch, &src_tog_flg, &dst_tog_flg);

    // Set channel source address which configure pipo mode
    if ((src != NULL) && (ctrl_val & GPDMA_CH_CTRL_SRC_PIPO_MODE_BIT)){
        if (src_tog_flg == 0) { // ping block
            SET_DMA_CH_ADDR(SRC_ADDR0, channel, src);
        } else { // pong block
            SET_DMA_CH_ADDR(SRC_ADDR1, channel, src);
        }
    }

    if ((dst != NULL) && (ctrl_val & GPDMA_CH_CTRL_DST_PIPO_MODE_BIT)){
        if (dst_tog_flg == 0) { // ping block
            SET_DMA_CH_ADDR(DST_ADDR0, channel, dst);
        } else { // pong block
            SET_DMA_CH_ADDR(DST_ADDR1, channel, dst);
        }
    }

    // Set channel length register
#if GPDMAC_ARCS_D0
    if (sample_len > 0) {
        uint32_t* ch_len;
        if (src_tog_flg || dst_tog_flg) {
            gpdma_ch_info[channel].length_po = sample_len;
            ch_len = (uint32_t*)&IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all;
        } else {
            gpdma_ch_info[channel].length = sample_len;
            ch_len = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all;
        }
        ch_len += channel;
        *ch_len = sample_len;
    }

    if (flags & 1) { // Stop PingPing after this block transfer
        // Set stop mode to zero (stop after this block transfer)
        ctrl_val &= ~GPDMA_CH_CTRL_IMMEDIATE_STOP_BIT;

        // Set channel stop bit to 1
        ctrl_val |= GPDMA_CH_CTRL_STOP_BIT;

        SET_DMA_CH_CTRL(channel, ctrl_val);
    }
#endif // GPDMAC_ARCS_D0

    return CSK_DRIVER_OK;
}


int32_t
GPDMA_Stop(csk_gpdma_ch_t ch){
    uint8_t channel = ch;

/*
    if (gpdma_ch_info[channel].status != gpdma_status_busy){
        return CSK_GPDMA_STATUS_ERROR;
    }

    uint32_t* channel_control = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + channel;

    // Set stop mode to zero
    *channel_control &= (~(0x1 << GPDMA_CH_CTRL_STOP_MODE_OFFSET));

    // Set stop mode to 1 (stop right now)
    //*channel_control |= (0x1 << GPDMA_CH_CTRL_STOP_MODE_OFFSET);

    // Set channel stop bit to 1
    *channel_control |= (0x1 << GPDMA_CH_CTRL_STOP_OFFSET);

    // Mask channel block interrupt
    *channel_control |= GPDMA_CH_CTRL_BLOCK_MASK_BIT;
*/

    uint32_t ctrl_val = GET_DMA_CH_CTRL(channel);

    // Stop immediately & Mask block interrupt
    ctrl_val |= GPDMA_CH_CTRL_IMMEDIATE_STOP_BIT | GPDMA_CH_CTRL_STOP_BIT | GPDMA_CH_CTRL_BLOCK_MASK_BIT;
    SET_DMA_CH_CTRL(channel, ctrl_val);

    // Clear interrupt
#if GPDMAC_ARCS_D0
    IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_BLOCK_FINISH_CLR = 0x1 << channel;
    IP_GPDMA->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x1 << channel;
#else
    IP_GPDMA->REG_DMA_BLOCK_FINISH_CLR.bit.CFG_BLOCK_FINISH_CLR = 0x1 << channel;
#endif

    gpdma_ch_info[channel].status = gpdma_status_config;

    return CSK_DRIVER_OK;
}

int32_t
GPDMA_Resume(csk_gpdma_ch_t ch){
    uint8_t channel = ch;

/*
    if (gpdma_ch_info[channel].status != gpdma_status_stopped){
        return CSK_GPDMA_STATUS_ERROR;
    }

    uint32_t* channel_control = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + channel;

    // Unmask channel block interrupt
    *channel_control &= ~GPDMA_CH_CTRL_BLOCK_MASK_BIT;

    gpdma_ch_info[channel].status = gpdma_status_busy;

    // Start
    *channel_control |= 0x2;
*/

    // Check if the channel is busy & configured or not
    if (gpdma_ch_busy(ch) || gpdma_ch_info[channel].status != gpdma_status_config)
        return CSK_GPDMA_STATUS_ERROR;

    uint32_t ctrl_val = GET_DMA_CH_CTRL(channel);

    // Unmask block interrupt
    ctrl_val &= ~GPDMA_CH_CTRL_BLOCK_MASK_BIT;

    // Enable & Start
    ctrl_val |= GPDMA_CH_CTRL_START_BIT | GPDMA_CH_CTRL_CH_EN_BIT;
    SET_DMA_CH_CTRL(channel, ctrl_val);

    return CSK_DRIVER_OK;
}

/*
uint32_t
GPDMA_GetCnt(csk_gpdma_ch_t ch, uint32_t* sample_len){
    uint8_t channel = ch;

    // Current channel have stopped
    if ((gpdma_ch_info[channel].status == gpdma_status_config)){
        // Have transmit complete
        if (gpdma_ch_info[channel].xfer_length == gpdma_ch_info[channel].length){
            *sample_len = gpdma_ch_info[channel].length;
        }
        // Have residue size
        else {
            uint16_t residue_len = 0;

            // Block length
            uint32_t* channel_length = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all + channel;

            // Choose remain length register
            IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_REMAIN_CNT_MODE;
            IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

            residue_len = IP_GPDMA->REG_DMA_DIAG_RPT.all & GPDMA_CH_MAX_BLOCK_LENGTH_MASK;

            if (*channel_length < residue_len){
                return CSK_GPDMA_UNKNOWN_ERROR;
            }

            *sample_len = gpdma_ch_info[channel].xfer_length + (*channel_length - residue_len);
        }
    }

    // Current channel still running
    else if (gpdma_ch_info[channel].status == gpdma_status_busy){
        *sample_len = gpdma_ch_info[channel].xfer_length;
    }

    else {
        return CSK_GPDMA_STATUS_ERROR;
    }

    return CSK_DRIVER_OK;
}
*/


static uint32_t gpdma_get_block_cnt(csk_gpdma_ch_t ch)
{
    uint32_t *block_len_p;
#if GPDMAC_ARCS_D0 // ARCS D0 and later
    block_len_p = (uint32_t *)(gpdma_get_pipo_sel(ch, NULL) ?
                            &IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all :
                            &IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all);
#else // ARCS C0
    block_len_p = (uint32_t *)(&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all);
#endif
    return *(block_len_p + ch);
}

static uint32_t gpdma_get_remain_cnt(csk_gpdma_ch_t ch)
{
    // Choose remain length register
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_REMAIN_CNT_MODE;
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = ch;

    return (IP_GPDMA->REG_DMA_DIAG_RPT.all & GPDMA_CH_MAX_BLOCK_LENGTH_MASK);
}

static uint32_t gpdma_get_total_cnt(csk_gpdma_ch_t ch)
{
    uint32_t residue_len = gpdma_get_remain_cnt(ch); // Block remain length
    uint32_t channel_length = gpdma_get_block_cnt(ch); // Block length
    uint32_t sample_len = gpdma_ch_info[ch].xfer_length;

    if (channel_length < residue_len) {
        //FIXME: SHOULD NOT come here!!
        // it means that Ping / Pong switch has just occurred?
        return sample_len;
    }

    // if residue_len == 0 it means block transfer ISR has been called,
    // so the block length has been calculated into the xfer_length
    if (residue_len > 0)
        sample_len += channel_length - residue_len;
    return sample_len;
}

uint32_t
GPDMA_GetCnt(csk_gpdma_ch_t ch, uint32_t* sample_len){
    uint8_t channel = ch;

    // Current channel have stopped
    if ((gpdma_ch_info[channel].status == gpdma_status_config)){
        // Have transmit complete
#if !GPDMAC_ARCS_D0 // ARCS_C0
        if (gpdma_ch_info[channel].xfer_length == gpdma_ch_info[channel].length){
            *sample_len = gpdma_ch_info[channel].length;
        } else
#endif
        // Have residue size
        *sample_len = gpdma_get_total_cnt(ch);
    }

    // Current channel still running
    else if (gpdma_ch_info[channel].status == gpdma_status_busy){
        *sample_len = gpdma_get_total_cnt(ch);
    }

    else {
        return CSK_GPDMA_STATUS_ERROR;
    }

    return CSK_DRIVER_OK;
}


// pipo_sel: 0 indicates ping block, 1 indicates pong block, -1 indicate last done block (ping or pong)
// src/dst : return source/destination address of the ping/pong block if NOT NULL
uint32_t GPDMA_GetCnt_PiPoBlk(csk_gpdma_ch_t ch, int8_t pipo_sel, uint32_t *src, uint32_t *dst)
{
    uint32_t *item_p;

    // select ping or pong if last done is specified
    if (pipo_sel == -1)
        pipo_sel = gpdma_get_pipo_sel(ch, NULL);

    // source address
    if (src != NULL) {
        item_p = (uint32_t *)(pipo_sel ?
                &IP_GPDMA->REG_DMA_SRC_ADDR1_CH0.all :
                &IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all);
        item_p += ch * 4;
        if (item_p >= &IP_GPDMA->REG_RESV_0XA0_0XFC[0])
            item_p += ARRAY_COUNT(IP_GPDMA->REG_RESV_0XA0_0XFC);
        *src = *item_p;
    }

    // destination address
    if (dst != NULL) {
        item_p = (uint32_t *)(pipo_sel ?
                &IP_GPDMA->REG_DMA_DST_ADDR1_CH0.all :
                &IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all);
        item_p += ch * 4;
        if (item_p >= &IP_GPDMA->REG_RESV_0XA0_0XFC[0])
            item_p += ARRAY_COUNT(IP_GPDMA->REG_RESV_0XA0_0XFC);
        *dst = *item_p;
    }

    // block length
#if GPDMAC_ARCS_D0 // ARCS D0 and later
    item_p = (uint32_t *)(pipo_sel ?
                            &IP_GPDMA->REG_DMA_PO_BLOCK_LEN_CH_00.all :
                            &IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all);
#else // ARCS C0
    item_p = (uint32_t *)(&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all);
#endif

    return *(item_p + ch);
}


int32_t
GPDMA_GetStatus(csk_gpdma_ch_t ch, csk_gpdma_info_t* info){
    uint8_t channel = ch;

    info->status = gpdma_ch_info[channel].status;

    // Get current source address and destination address
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

    // Source address
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_SRC_ADDRESS_MODE;

    info->cur_src_addr = IP_GPDMA->REG_DMA_DIAG_RPT.all;

    // Destination address
    IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_DST_ADDRESS_MODE;

    info->cur_dst_addr = IP_GPDMA->REG_DMA_DIAG_RPT.all;

    return CSK_DRIVER_OK;
}

static void
GPDMA_IRQ_Handler(void){
    // Clear interrupt pending
#if GPDMAC_ARCS_D0
    uint32_t int_status = IP_GPDMA->REG_DMA_INT_STATUS.bit.BLOCK_FINISH_STATUS;
    // IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_BLOCK_FINISH_CLR = int_status;
#else
    uint32_t int_status = IP_GPDMA->REG_DMA_BLOCK_FINISH_STATUS.bit.BLOCK_FINISH_STATUS;
    // IP_GPDMA->REG_DMA_BLOCK_FINISH_CLR.bit.CFG_BLOCK_FINISH_CLR = int_status;
#endif

    // Get channel register
    uint32_t* channel_control = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all;
    uint32_t* channel_length = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all;
//    uint32_t* src_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all;
//    uint32_t* dst_address0 = (uint32_t*)&IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all;
    uint32_t *src_address0, *dst_address0;

    csk_gpdma_ch_info_t* p_channel = NULL;

    uint8_t channel = 0;

    uint32_t event = 0;

    while(int_status){
        event = 0;

        // Corresponding interrupt trigger
        if (int_status & 0x1){

            if (gpdma_ch_info[channel].status != gpdma_status_busy) {
                int_status = int_status >> 1;
                channel++;
                continue;
            }
            IP_GPDMA->REG_DMA_INT_CLR.bit.CFG_BLOCK_FINISH_CLR = (1 << channel);

            channel_control = (uint32_t*)&IP_GPDMA->REG_DMA_CH0_CTRL.all + channel;

            channel_length = (uint32_t*)&IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all + channel;

/*
            // TODO This is a BUG for address fill error in EXCEL
            // Get channel source and destination register
            src_address0 = (uint32_t *)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all;
            dst_address0 = (uint32_t *)&IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all;

            if (channel >= gp_dma_ch5){
                // Get channel source and destination register
                src_address0 = (uint32_t *)&IP_GPDMA->REG_DMA_SRC_ADDR0_CH5.all;
                dst_address0 = (uint32_t *)&IP_GPDMA->REG_DMA_DST_ADDR0_CH5.all;

                src_address0 += (channel - gp_dma_ch5) * 4;
                dst_address0 += (channel - gp_dma_ch5) * 4;
            } else {
                src_address0 += channel * 4;
                dst_address0 += channel * 4;
            }
*/

            p_channel = (csk_gpdma_ch_info_t*)&gpdma_ch_info[channel];

            // **************** Normal mode
            if (p_channel->mode == address_mode_normal){
                // Need transfer residue data
                if ((p_channel->length - p_channel->xfer_length) > GPDMA_CH_MAX_BLOCK_LENGTH){

                    p_channel->xfer_length += *channel_length;

                    // ****************Length
                    // Calculate residue size
                    if ((p_channel->length - p_channel->xfer_length) >= GPDMA_CH_MAX_BLOCK_LENGTH){
                        *channel_length = GPDMA_CH_MAX_BLOCK_LENGTH;
                    } else {
                        *channel_length = (p_channel->length - p_channel->xfer_length);
                    }

                    // ****************Source
                    // Increase mode
                    if (!(*channel_control & (0x1 << GPDMA_CH_CTRL_SRC_INC_MODE_OFFSET))){
                        GET_DMA_CH_ADDR(SRC_ADDR0, channel, src_address0);
                        *src_address0 = *src_address0 + GPDMA_CH_MAX_BLOCK_LENGTH;
                    }

                    // ****************Destination
                    // Increase mode
                    if (!(*channel_control & (0x1 << GPDMA_CH_CTRL_DST_INC_MODE_OFFSET))){
                        GET_DMA_CH_ADDR(DST_ADDR0, channel, dst_address0);
                        *dst_address0 = *dst_address0 + GPDMA_CH_MAX_BLOCK_LENGTH;
                    }

                    // ****************Start
                    *channel_control |= 0x2;
                }

                // It's the last block
                else {
                    // Transmit complete
                    p_channel->xfer_length = p_channel->length;

                    event |= CSK_GPDMA_EVENT_TRANSFER_DONE;

                    // Change status to <config>
                    p_channel->status = gpdma_status_config;
                }

            }

            // **************** PIPO mode
            if (p_channel->mode == address_mode_pipo){
                uint8_t ch_busy;
                uint8_t is_pong = gpdma_get_pipo_sel(channel, &ch_busy);

                // Increase length
            #if GPDMAC_ARCS_D0
                p_channel->xfer_length += is_pong ? p_channel->length_po : p_channel->length;
            #else
                p_channel->xfer_length += p_channel->length;
            #endif

                //IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = GPDMA_DIAG_SEL_TOG_FLAG_MODE;
                //IP_GPDMA->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

                //if ((IP_GPDMA->REG_DMA_DIAG_RPT.all & 0x300000) >> 20){
                if (is_pong) {
                    event |= CSK_GPDMA_EVENT_PIPO1_DONE;
                } else {
                    event |= CSK_GPDMA_EVENT_PIPO0_DONE;
                }

                // notify transfer is done when channel is NOT busy
                if (!ch_busy) {
                    event |= CSK_GPDMA_EVENT_TRANSFER_DONE;

                    // Change status to <config>
                    p_channel->status = gpdma_status_config;
                }
            }

            // Callback
            if (p_channel->cb_event != NULL && event != 0){
                p_channel->cb_event(event, p_channel->workspace);
            }

//            IP_GPDMA->REG_DMA_CH_CLR.all |= (0x1 << channel); //FIXME:
//            CLOGD("Ch %d\n", channel);
        }

        int_status = int_status >> 1;

        // increase channel number
        channel++;
    }
}
