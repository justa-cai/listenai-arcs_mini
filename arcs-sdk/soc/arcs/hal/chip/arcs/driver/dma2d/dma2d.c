#include <string.h> // for memset etc.
#include "ClockManager.h"
#include "log_print.h"
#include "arcs_ap.h"

#include "Driver_DMA2D.h"

// DMA2D Control register
#define DMA2D_CH_CTRL_HS_SEL_OFFSET         28
#define DMA2D_CH_CTRL_CFG_SGEN_OFFSET       27
#define DMA2D_CH_CTRL_CFG_DSEN_OFFSET       26
#define DMA2D_CH_CTRL_CH_PRIO_OFFSET        24
#define DMA2D_CH_CTRL_PRE_FETCH_OFFSET      23
#define DMA2D_CH_CTRL_FORCE_ON_OFFSET       22
#define DMA2D_CH_CTRL_HALF_BLOCK_OFFSET     21
#define DMA2D_CH_CTRL_BLOCK_OFFSET          20
#define DMA2D_CH_CTRL_READ_DONE_ACK_OFFSET  19
#define DMA2D_CH_CTRL_FLOW_CTRL_OFFSET      18
#define DMA2D_CH_CTRL_DST_BURST_OFFSET      16
#define DMA2D_CH_CTRL_SRC_BURST_OFFSET      14
#define DMA2D_CH_CTRL_AUTO_TFR_OFFSET       13
#define DMA2D_CH_CTRL_DST_PIPO_MODE_OFFSET  12
#define DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET  11
#define DMA2D_CH_CTRL_DST_INC_MODE_OFFSET   10
#define DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET   9
#define DMA2D_CH_CTRL_SAMPLE_UNIT_OFFSET    6
#define DMA2D_CH_CTRL_TFR_MODE_OFFSET       4
#define DMA2D_CH_CTRL_STOP_MODE_OFFSET      3
#define DMA2D_CH_CTRL_STOP_OFFSET           2
#define DMA2D_CH_CTRL_CH_START_OFFSET       1
#define DMA2D_CH_CTRL_CH_EN_OFFSET          0

// DMA2D DIAGRAM CONFIGURE
#define DMA2D_DIAG_SEL_TOG_FLAG_MODE        0
#define DMA2D_DIAG_SEL_REMAIN_CNT_MODE      0
#define DMA2D_DIAG_SEL_SRC_ADDRESS_MODE     4
#define DMA2D_DIAG_SEL_DST_ADDRESS_MODE     5

// DMA2D BYTES SIZE
#define DMA2D_OUT_FORMAT_ARGB_BYTES_SIZE    4
#define DMA2D_OUT_FORMAT_XRGB_BYTES_SIZE    3
#define DMA2D_OUT_FORMAT_XBGR_BYTES_SIZE    3
#define DMA2D_OUT_FORMAT_YUV422_BYTES_SIZE  2
#define DMA2D_OUT_FORMAT_Y8_BYTES_SIZE  	1

#define DMA2D_IN_FORMAT_YUV444_BYTES_SIZE   4
#define DMA2D_IN_FORMAT_ARGB_BYTES_SIZE   	4
#define DMA2D_IN_FORMAT_ABGR_BYTES_SIZE   	4
#define DMA2D_IN_FORMAT_XRGB_BYTES_SIZE   	3
#define DMA2D_IN_FORMAT_XBGR_BYTES_SIZE   	3
#define DMA2D_IN_FORMAT_YUV422_BYTES_SIZE   2
#define DMA2D_IN_FORMAT_YUV420_BYTES_SIZE   2

#define DMA2D_IMAGE_CROPPED      			1
#define DMA2D_IMAGE_NOT_CROPPED  			0

#define DMA2D_IMAGER_ZOOM_SCALE 			1
#define DMA2D_IMAGER_NOT_ZOOM_SCALE 		0

// DMA2D MAX BLOCK
#define DMA2D_CH_MAX_BLOCK_LENGTH           0xFFFFF //20bit
#define DMA2D_CH_MAX_BLOCK_LENGTH_MASK      0xFFFFF

#define DMA2D_CH_IMG_MAX_WIDTH              0x1FFF
#define DMA2D_CH_IMG_MAX_HEIGHT             0x1FFF

typedef struct _csk_dma2d_ch_info {
    CSK_DMA2D_SignalEvent_t cb_event;
    void* workspace;
    csk_dma2d_status_t status;
    // In normal mode: total length
    // In PiPo mode: buffer length
    uint32_t length;
    // Current transfer length
    uint32_t xfer_length;
    //crop offset
    uint32_t crop_offset;
    // transfer mode
    uint32_t mode;
    csk_jpeg_2d_type_t mode_2d;
    uint8_t block_in_size;
    uint8_t block_out_size;
    bool is_cropped;
    bool is_zoom_scale;
    csk_dma2d_ch_t trigger_ch;
} csk_dma2d_ch_info_t;

static csk_dma2d_ch_info_t dma2d_ch_info[CSK_DMA2D_MAX_CHANNEL_NUM] = {0};

static volatile uint8_t DMA2D_GLB_FLAG = 0;

static void DMA2D_IRQ_Handler(void);

int32_t
DMA2D_Stop(csk_dma2d_ch_t ch) {
    uint8_t channel = ch;

/*
    if (dma2d_ch_info[channel].status != dma2d_status_busy) {
        return CSK_DMA2D_STATUS_ERROR;
    }
*/

    uint32_t* channel_control = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all + channel;

    // Set Stop mode to zero
    *channel_control &= (~(0x1 << DMA2D_CH_CTRL_STOP_MODE_OFFSET));

    // Set channel stop bit to 1
    *channel_control |= (0x1 << DMA2D_CH_CTRL_STOP_OFFSET);

    // Clear interrupt
    IP_DMA2D->REG_DMA_IMAGE_INT_CLR.bit.CFG_IMAGE_BLOCK_FINISH_CLR = 0x1 << (channel - CSK_GPDMA_MAX_CHANNEL_NUM);
    IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = (1 << channel);

    dma2d_ch_info[channel].status = dma2d_status_config;

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_Config(csk_dma2d_init_t* res, CSK_DMA2D_SignalEvent_t cb_event, void* workspace) {
    if ((dma2d_ch_info[res->dma_ch].status == dma2d_status_none) || (dma2d_ch_info[res->dma_ch].status == dma2d_status_busy)){
        return CSK_DMA2D_STATUS_ERROR;
    }

    dma2d_ch_info[res->dma_ch].cb_event = cb_event;
    dma2d_ch_info[res->dma_ch].workspace = workspace;
    dma2d_ch_info[res->dma_ch].trigger_ch = res->trigger_ch;

    uint8_t channel = (uint8_t)res->dma_ch;
    if (channel >= CSK_DMA2D_MAX_CHANNEL_NUM) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (dma_2d_ch6 <= res->trigger_ch && dma_2d_ch9 >= res->trigger_ch)
    {
        IP_DMA2D->REG_DMA_CH_TRIGGER_CTRL.all |= (((res->trigger_ch - dma_2d_ch6)<<((res->dma_ch - dma_2d_ch6) * 2))|((0x1 << (res->dma_ch - dma_2d_ch6))<<8));
    }

    // Get channel control base address
    uint32_t* ch_ctrl = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    ch_ctrl += channel;

    {
        uint32_t ch_ctrl_para = 0;

        // channel prio configure
        {
            ch_ctrl_para |= (res->prio_lvl << DMA2D_CH_CTRL_CH_PRIO_OFFSET);
        }

        // burst length config
        {
            ch_ctrl_para |= (res->burst_len << DMA2D_CH_CTRL_SRC_BURST_OFFSET | res->burst_len << DMA2D_CH_CTRL_DST_BURST_OFFSET);
        }

        // enable rd_done_ack
        {
        	ch_ctrl_para |= (res->rd_done_ack << DMA2D_CH_CTRL_READ_DONE_ACK_OFFSET);
        }

        // normal mode or PIPO mode
        {
            uint32_t src_mode, dst_mode;

            src_mode = res->src_mode;
            dst_mode = res->dst_mode;

            // If one mode in PIPO mode, need configure auto transfer
            if (src_mode || dst_mode){
                ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_AUTO_TFR_OFFSET);
            }

            ch_ctrl_para |= (src_mode << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET | dst_mode << DMA2D_CH_CTRL_DST_PIPO_MODE_OFFSET);
        }

        // address mode configure
        {
            uint32_t src_add_mode, dst_add_mode;

            src_add_mode = res->src_inc_mode;
            dst_add_mode = res->dst_inc_mode;

            ch_ctrl_para |= (src_add_mode << DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET | dst_add_mode << DMA2D_CH_CTRL_DST_INC_MODE_OFFSET);
        }

        // transfer mode
        {
            ch_ctrl_para |= (res->tfr_mode << DMA2D_CH_CTRL_TFR_MODE_OFFSET);

            // configure handshake
            if (res->tfr_mode != tfr_mode_m2m) {
                // Clear flow control

                if (res->handshake == hs_none) {
                    return CSK_DRIVER_ERROR_PARAMETER;
                }

                if (res->handshake >= hs_max) {
                    return CSK_DRIVER_ERROR_PARAMETER;
                }

                ch_ctrl_para |= (res->handshake & 0xF) << DMA2D_CH_CTRL_HS_SEL_OFFSET;
                if (dma2d_flow_ctrl_peripheral == res->flow_ctrl)
                {
                    ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_FLOW_CTRL_OFFSET);
                }
            } else {
                // Flow control
                ch_ctrl_para |= (0x0 << DMA2D_CH_CTRL_FLOW_CTRL_OFFSET);
            }
        }
        // Sample unit
        {
            // Configure source unit is same with destination unit
            ch_ctrl_para |= (res->sample_unit  << DMA2D_CH_CTRL_SAMPLE_UNIT_OFFSET);
            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all &= ~(0x3 << channel * 2);
            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all |= (res->sample_unit << channel * 2);
        }

        ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_CH_EN_OFFSET);

        *ch_ctrl = ch_ctrl_para;
    }
    dma2d_ch_info[res->dma_ch].status = dma2d_status_config;

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_Rotate_Config(csk_dma_2d_rotate_cfg_t* res0, csk_dma_2d_rotate_cfg_t* res1, CSK_DMA2D_SignalEvent_t cb_event_res0, CSK_DMA2D_SignalEvent_t cb_event_res1, void* workspace_src0, void *workspace_src1) {
    if ((dma2d_ch_info[res0->dma2d_init.dma_ch].status == dma2d_status_none) || (dma2d_ch_info[res0->dma2d_init.dma_ch].status == dma2d_status_busy)){
        return CSK_DMA2D_STATUS_ERROR;
    }

    if ((dma2d_ch_info[res1->dma2d_init.dma_ch].status == dma2d_status_none) || (dma2d_ch_info[res1->dma2d_init.dma_ch].status == dma2d_status_busy)){
        return CSK_DMA2D_STATUS_ERROR;
    }

    dma2d_ch_info[res0->dma2d_init.dma_ch].cb_event = cb_event_res0;
    dma2d_ch_info[res0->dma2d_init.dma_ch].workspace = workspace_src0;

    dma2d_ch_info[res1->dma2d_init.dma_ch].cb_event = cb_event_res1;
    dma2d_ch_info[res1->dma2d_init.dma_ch].workspace = workspace_src1;

    uint8_t channel_res0 = (uint8_t)res0->dma2d_init.dma_ch;
    if (channel_res0 >= CSK_DMA2D_MAX_CHANNEL_NUM) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t channel_res1 = (uint8_t)res1->dma2d_init.dma_ch;
    if (channel_res1 >= CSK_DMA2D_MAX_CHANNEL_NUM) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Get channel control base address
    uint32_t* ch_ctrl_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    uint32_t* ch_ctrl_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;

    ch_ctrl_res0 += channel_res0;
    ch_ctrl_res1 += channel_res1;

    {
    	uint32_t ch_ctrl_para_res0 = 0;
    	uint32_t ch_ctrl_para_res1 = 0;

        // channel prio configure
        {
            ch_ctrl_para_res0 |= (res0->dma2d_init.prio_lvl << DMA2D_CH_CTRL_CH_PRIO_OFFSET);
            ch_ctrl_para_res1 |= (res1->dma2d_init.prio_lvl << DMA2D_CH_CTRL_CH_PRIO_OFFSET);
        }

        // burst length config
        {
            ch_ctrl_para_res0 |= (res0->dma2d_init.burst_len << DMA2D_CH_CTRL_SRC_BURST_OFFSET | res0->dma2d_init.burst_len << DMA2D_CH_CTRL_DST_BURST_OFFSET);
            ch_ctrl_para_res1 |= (res1->dma2d_init.burst_len << DMA2D_CH_CTRL_SRC_BURST_OFFSET | res1->dma2d_init.burst_len << DMA2D_CH_CTRL_DST_BURST_OFFSET);
        }

        {
        	ch_ctrl_para_res0 |= (0x2 << DMA2D_CH_CTRL_HS_SEL_OFFSET);
        	ch_ctrl_para_res1 |= (0x2 << DMA2D_CH_CTRL_HS_SEL_OFFSET);
        }

        // enable rd_done_ack
        {
        	ch_ctrl_para_res0 |= (res0->dma2d_init.rd_done_ack << DMA2D_CH_CTRL_READ_DONE_ACK_OFFSET);
        	ch_ctrl_para_res1 |= (res1->dma2d_init.rd_done_ack << DMA2D_CH_CTRL_READ_DONE_ACK_OFFSET);
        }

        // normal mode or PIPO mode
        {
            uint32_t src_mode_res0, dst_mode_res0;
            uint32_t src_mode_res1, dst_mode_res1;

            src_mode_res0 = res0->dma2d_init.src_mode;
            dst_mode_res0 = res0->dma2d_init.dst_mode;

            src_mode_res1 = res1->dma2d_init.src_mode;
            dst_mode_res1 = res1->dma2d_init.dst_mode;

            // If one mode in PIPO mode, need configure auto transfer
            if (src_mode_res0 || dst_mode_res0){
                ch_ctrl_para_res0 |= (0x1 << DMA2D_CH_CTRL_AUTO_TFR_OFFSET);
            }

            if (src_mode_res1 || dst_mode_res1){
                ch_ctrl_para_res1 |= (0x1 << DMA2D_CH_CTRL_AUTO_TFR_OFFSET);
            }

            ch_ctrl_para_res0 |= (src_mode_res0 << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET | dst_mode_res0 << DMA2D_CH_CTRL_DST_PIPO_MODE_OFFSET);
            ch_ctrl_para_res1 |= (src_mode_res1 << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET | dst_mode_res1 << DMA2D_CH_CTRL_DST_PIPO_MODE_OFFSET);
        }

        // address mode configure
        {
            uint32_t src_add_mode_res0, dst_add_mode_res0;
            uint32_t src_add_mode_res1, dst_add_mode_res1;

            src_add_mode_res0 = res0->dma2d_init.src_inc_mode;
            dst_add_mode_res0 = res0->dma2d_init.dst_inc_mode;

            src_add_mode_res1 = res1->dma2d_init.src_inc_mode;
            dst_add_mode_res1 = res1->dma2d_init.dst_inc_mode;

            ch_ctrl_para_res0 |= (src_add_mode_res0 << DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET | dst_add_mode_res0 << DMA2D_CH_CTRL_DST_INC_MODE_OFFSET);
            ch_ctrl_para_res1 |= (src_add_mode_res1 << DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET | dst_add_mode_res1 << DMA2D_CH_CTRL_DST_INC_MODE_OFFSET);
        }

        // transfer mode
		{
			ch_ctrl_para_res0 |= (res0->dma2d_init.tfr_mode << DMA2D_CH_CTRL_TFR_MODE_OFFSET);

			// configure handshake
			if (res0->dma2d_init.tfr_mode != tfr_mode_m2m) {
				// Clear flow control

				if (res0->dma2d_init.handshake == hs_none) {
					return CSK_DRIVER_ERROR_PARAMETER;
				}

				if (res0->dma2d_init.handshake >= hs_max) {
					return CSK_DRIVER_ERROR_PARAMETER;
				}

				// DMA SELECT 1
				if ((res0->dma2d_init.handshake & 0x10) == 0x10) {
					IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x1;
				} else { // DMA SELECT 0
					IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x0;
				}

				ch_ctrl_para_res0 |= (res0->dma2d_init.handshake & 0xF) << DMA2D_CH_CTRL_HS_SEL_OFFSET;
			} else {
				// Flow control
				ch_ctrl_para_res0 |= (0x0 << DMA2D_CH_CTRL_FLOW_CTRL_OFFSET);
			}

			ch_ctrl_para_res1 |= (res1->dma2d_init.tfr_mode << DMA2D_CH_CTRL_TFR_MODE_OFFSET);

			// configure handshake
			if (res1->dma2d_init.tfr_mode != tfr_mode_m2m) {
				// Clear flow control

				if (res1->dma2d_init.handshake == hs_none) {
					return CSK_DRIVER_ERROR_PARAMETER;
				}

				if (res1->dma2d_init.handshake >= hs_max) {
					return CSK_DRIVER_ERROR_PARAMETER;
				}

				// DMA SELECT 1
				if ((res1->dma2d_init.handshake & 0x10) == 0x10) {
					IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x1;
				} else { // DMA SELECT 0
					IP_AP_CFG->REG_DMA_SEL.bit.DMA_SEL = 0x0;
				}

				ch_ctrl_para_res1 |= (res1->dma2d_init.handshake & 0xF) << DMA2D_CH_CTRL_HS_SEL_OFFSET;
			} else {
				// Flow control
				ch_ctrl_para_res1 |= (0x0 << DMA2D_CH_CTRL_FLOW_CTRL_OFFSET);
			}
		}

        // Sample unit
        {
            // Configure source unit is same with destination unit
            ch_ctrl_para_res0 |= (res0->dma2d_init.sample_unit  << DMA2D_CH_CTRL_SAMPLE_UNIT_OFFSET);
            ch_ctrl_para_res1 |= (res1->dma2d_init.sample_unit  << DMA2D_CH_CTRL_SAMPLE_UNIT_OFFSET);

            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all &= ~(0x3 << channel_res0 * 2);
            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all |= (res0->dma2d_init.sample_unit << channel_res0 * 2);

            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all &= ~(0x3 << channel_res1 * 2);
            IP_DMA2D->REG_DMA_DST_TRANS_BASE_UNIT.all |= (res1->dma2d_init.sample_unit << channel_res1 * 2);
        }

        ch_ctrl_para_res0 |= (0x1 << DMA2D_CH_CTRL_CH_EN_OFFSET);
        ch_ctrl_para_res1 |= (0x1 << DMA2D_CH_CTRL_CH_EN_OFFSET);

        *ch_ctrl_res0 = ch_ctrl_para_res0;
        *ch_ctrl_res1 = ch_ctrl_para_res1;
    }

    dma2d_ch_info[res0->dma2d_init.dma_ch].status = dma2d_status_config;
    dma2d_ch_info[res1->dma2d_init.dma_ch].status = dma2d_status_config;

    return CSK_DRIVER_OK;
}



int32_t
DMA2D_Start_Normal(csk_dma2d_ch_t ch, void* src, void* dst, uint32_t img_in_len) {
    uint8_t channel = ch;

    // Only in configure status can start normal transmits
    if ((dma2d_ch_info[channel].status != dma2d_status_config) && (dma2d_ch_info[channel].status != dma2d_status_config_img)) {
        return CSK_DMA2D_STATUS_ERROR;
    }

    // Get channel control register
    uint32_t* ch_ctrl = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    ch_ctrl += channel;

    // Get channel length register
    uint32_t* ch_img_in_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all;
    ch_img_in_len_reg += channel;

    // source mode and destination mode must both be normal mode
    if ((*ch_ctrl & (0x3 << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET)) != 0x0){
        return CSK_DMA2D_MODE_ERROR;
    }

    uint32_t* src_address = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* dst_address = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;

    // TODO This is a BUG for address fill error in EXCEL
    // Get channel source and destination register
    {
        if (channel >= dma_2d_ch6) {
            // Get channel source and destination register
            src_address = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH6.all;
            dst_address = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH6.all;

            src_address += (channel - dma_2d_ch6) * 4;
            dst_address += (channel - dma_2d_ch6) * 4;
        } else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // Configure source and destination
    if(dma2d_ch_info[channel].is_cropped == DMA2D_IMAGE_CROPPED) {
    	*src_address = (uint32_t)src + dma2d_ch_info[channel].crop_offset;
    } else {
    	*src_address = (uint32_t)src;
    }

    *dst_address = (uint32_t)dst;

    // Configure length
    dma2d_ch_info[channel].length = img_in_len;
    dma2d_ch_info[channel].xfer_length = 0;
    if (img_in_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
        *ch_img_in_len_reg = DMA2D_CH_MAX_BLOCK_LENGTH;
    } else {
        *ch_img_in_len_reg = img_in_len;
    }

	uint32_t* ch_img_out_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
	if(channel == dma_2d_ch6)
		ch_img_out_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
	else if(channel == dma_2d_ch7)
		ch_img_out_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all;
	else if(channel == dma_2d_ch8)
		ch_img_out_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all;
	else if(channel == dma_2d_ch9)
		ch_img_out_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.all;
	else
		return CSK_DRIVER_ERROR_PARAMETER;

	uint32_t img_out_len = 0;
	uint32_t img_out_width = 0;
	uint32_t img_out_height= 0;
    // Image function
    if (dma2d_ch_info[channel].status == dma2d_status_config_img) {
		if (dma2d_ch_info[channel].is_cropped == DMA2D_IMAGE_CROPPED) {
			 if(channel == dma_2d_ch6) {
				 img_out_width = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_WIDTH_OUT_CH6;
				 img_out_height = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_HEIGHT_OUT_CH6;

			 } else if(channel == dma_2d_ch7){
				 img_out_width = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_WIDTH_OUT_CH7;
				 img_out_height = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_HEIGHT_OUT_CH7;

			 } else if(channel == dma_2d_ch8) {
				 img_out_width = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_WIDTH_OUT_CH8;
				 img_out_height = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_HEIGHT_OUT_CH8;

			 }else {
				 img_out_width = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9;
				 img_out_height = IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9;

			 }

			 if(dma2d_ch_info[channel].is_zoom_scale == DMA2D_IMAGER_ZOOM_SCALE) //crop and zoom scale
			 {
				 img_in_len = img_out_width * img_out_height * dma2d_ch_info[ch].block_out_size / 4;
				 img_out_len = img_out_width * img_out_height * dma2d_ch_info[ch].block_out_size / 4 / ( dma2d_ch_info[ch].block_in_size / dma2d_ch_info[ch].block_out_size);

			 }
			 else // only crop
			 {
				 img_out_len = img_out_width * img_out_height * dma2d_ch_info[ch].block_in_size / 4;
				 img_in_len = img_out_len;
			 }

			 *ch_img_in_len_reg = img_in_len;
			 *ch_img_out_len_reg = img_out_len;
		} else {
	    	// Calculate the output block length
			img_out_len = img_in_len * dma2d_ch_info[ch].block_out_size / dma2d_ch_info[ch].block_in_size; //out_len = in_len * block_out_size/block_in_size

			if(img_out_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
				*ch_img_out_len_reg = DMA2D_CH_MAX_BLOCK_LENGTH;
			} else {
				*ch_img_out_len_reg = img_out_len;
			}
		}
    } else if (dma2d_ch_info[channel].status == dma2d_status_config) {
    // dma2d function img_out_len = img_in_len
    	img_out_len = img_in_len;
        if(img_out_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
            *ch_img_out_len_reg = DMA2D_CH_MAX_BLOCK_LENGTH;
        } else {
            *ch_img_out_len_reg = img_out_len;
        }
    } else {
    	return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Configure mode
    dma2d_ch_info[channel].mode = address_mode_normal;

    dma2d_ch_info[channel].status = dma2d_status_busy;

    // Enable finish interrupt
    IP_DMA2D->REG_DMA_IMAGE_INT_EN.bit.CFG_IMAGE_BLOCK_FINISH_INT_EN = 0x1;

    // Enable
    if (!(*ch_ctrl & 0x1)){
        *ch_ctrl |= 0x1;
    }

    // Start
    *ch_ctrl |= 0x2;

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_Start_Rotate(csk_dma_2d_rotate_cfg_t *res0, csk_dma_2d_rotate_cfg_t *res1, void* src0, void *dst1, uint32_t img_in_len) {
	uint8_t channel_res0 = res0->dma2d_init.dma_ch;
	uint8_t channel_res1 = res1->dma2d_init.dma_ch;

    if ((dma2d_ch_info[res0->dma2d_init.dma_ch].status != dma2d_status_config) && (dma2d_ch_info[res0->dma2d_init.dma_ch].status != dma2d_status_config_img)) {
        return CSK_DMA2D_STATUS_ERROR;
    }

    if ((dma2d_ch_info[res1->dma2d_init.dma_ch].status != dma2d_status_config) && (dma2d_ch_info[res1->dma2d_init.dma_ch].status != dma2d_status_config_img)) {
        return CSK_DMA2D_STATUS_ERROR;
    }

    // Get channel control register
    uint32_t* ch_ctrl_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    ch_ctrl_res0 += channel_res0;
    uint32_t* ch_ctrl_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    ch_ctrl_res1 += channel_res1;

    // Get channel length register
    uint32_t* ch_img_in_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all;
    ch_img_in_len_reg_res0 += channel_res0;
    uint32_t* ch_img_in_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all;
    ch_img_in_len_reg_res1 += channel_res1;

    // source mode and destination mode must both be normal mode
    if ((*ch_ctrl_res0 & (0x3 << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET)) != 0x0){
        return CSK_DMA2D_MODE_ERROR;
    }

    if ((*ch_ctrl_res1 & (0x3 << DMA2D_CH_CTRL_SRC_PIPO_MODE_OFFSET)) != 0x0){
        return CSK_DMA2D_MODE_ERROR;
    }

    uint32_t* src_address0_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* dst_address0_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;
    uint32_t* dst_address1_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR1_CH0.all;

	if(channel_res0 >= dma_2d_ch6) {
		src_address0_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH6.all;
		dst_address0_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH6.all;
		dst_address1_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR1_CH6.all;

		src_address0_res0 += (channel_res0 - dma_2d_ch6) * 4;
		dst_address0_res0 += (channel_res0 - dma_2d_ch6) * 4;
		dst_address1_res0 += (channel_res0 - dma_2d_ch6) * 4;
	} else {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	uint8_t* p_src = NULL;
	if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_clockwise_rotate) {
		p_src = (uint8_t *)src0 + res0->dma2d_img_cfg.img_width * (res0->dma2d_img_cfg.img_height - 1) * 2;
	} else if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_counterclockwise_rotate) {
		p_src = (uint8_t *)src0 + (res0->dma2d_img_cfg.img_width * 2 - 4);
	}

	uint8_t* p_dst = (uint8_t *)dst1;

	uint8_t* p_temp_0 = (uint8_t*)p_dst + (res0->dma2d_img_cfg.img_width * res0->dma2d_img_cfg.img_height * dma2d_ch_info[res1->dma2d_init.dma_ch].block_out_size);
	uint8_t* p_temp_1 = p_temp_0 + 8 * res0->dma2d_img_cfg.img_height * 2;

    // Configure source and destination
	*src_address0_res0 = (uint32_t)p_src;
	*dst_address0_res0 = (uint32_t)p_temp_0;
    *dst_address1_res0 = (uint32_t)p_temp_1;

    uint32_t* src_address0_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* src_address1_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR1_CH0.all;

    uint32_t* dst_address0_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;

    // TODO This is a BUG for address fill error in EXCEL
    // Get channel source and destination register
	if(channel_res1 >= dma_2d_ch6) {
		src_address0_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH6.all;
		src_address1_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR1_CH6.all;
		dst_address0_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH6.all;

		src_address0_res1 += (channel_res1 - dma_2d_ch6) * 4;
		src_address1_res1 += (channel_res1 - dma_2d_ch6) * 4;
		dst_address0_res1 += (channel_res1 - dma_2d_ch6) * 4;
	} else {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

    // Configure source and destination
    *src_address0_res1 = *dst_address0_res0;
    *src_address1_res1 = *dst_address1_res0;

    *dst_address0_res1 = (uint32_t)dst1;

    // Configure length
    dma2d_ch_info[res0->dma2d_init.dma_ch].length = img_in_len;
    dma2d_ch_info[res0->dma2d_init.dma_ch].xfer_length = 0;

    dma2d_ch_info[res1->dma2d_init.dma_ch].length = img_in_len;
    dma2d_ch_info[res1->dma2d_init.dma_ch].xfer_length = 0;

	uint32_t* ch_img_out_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
	if(channel_res0 == dma_2d_ch6)
		ch_img_out_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
	else if(channel_res0 == dma_2d_ch7)
		ch_img_out_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all;
	else if(channel_res0 == dma_2d_ch8)
		ch_img_out_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all;
	else if(channel_res0 == dma_2d_ch9)
		ch_img_out_len_reg_res0 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.all;
	else
		return CSK_DRIVER_ERROR_PARAMETER;

    if (img_in_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
        *ch_img_in_len_reg_res0 = DMA2D_CH_MAX_BLOCK_LENGTH;
    } else {
        *ch_img_in_len_reg_res0 = img_in_len;
    }

    uint32_t img_out_len_res0 = 0;
	if (dma2d_ch_info[res0->dma2d_init.dma_ch].status == dma2d_status_config_img) {
		img_out_len_res0 = img_in_len * dma2d_ch_info[res0->dma2d_init.dma_ch].block_out_size / dma2d_ch_info[res0->dma2d_init.dma_ch].block_in_size;

		if(img_out_len_res0 > DMA2D_CH_MAX_BLOCK_LENGTH) {
			*ch_img_out_len_reg_res0 = DMA2D_CH_MAX_BLOCK_LENGTH;
		} else {
			*ch_img_out_len_reg_res0 = img_out_len_res0;
		}
	}

	uint32_t* ch_img_out_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
	if(channel_res1 == dma_2d_ch6) {
		ch_img_out_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.bit.CFG_IMAGE_WIDTH_IN_CH6 = res0->dma2d_img_cfg.img_height;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_WIDTH_OUT_CH6 = res0->dma2d_img_cfg.img_height;

		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.bit.CFG_IMAGE_HEIGHT_IN_CH6 = res0->dma2d_img_cfg.img_width;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.bit.CFG_IMAGE_HEIGHT_OUT_CH6 = res0->dma2d_img_cfg.img_width;
	}else if(channel_res1 == dma_2d_ch7) {
		ch_img_out_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_WIDTH_IN_CH7 = res0->dma2d_img_cfg.img_height;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_WIDTH_OUT_CH7 = res0->dma2d_img_cfg.img_height;

		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH7.bit.CFG_IMAGE_HEIGHT_IN_CH7 = res0->dma2d_img_cfg.img_width;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH7.bit.CFG_IMAGE_HEIGHT_OUT_CH7 = res0->dma2d_img_cfg.img_width;
	}else if(channel_res1 == dma_2d_ch8) {
		ch_img_out_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8.bit.CFG_IMAGE_WIDTH_IN_CH8 = res0->dma2d_img_cfg.img_height;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_WIDTH_OUT_CH8 = res0->dma2d_img_cfg.img_height;

		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH8.bit.CFG_IMAGE_HEIGHT_IN_CH8 = res0->dma2d_img_cfg.img_width;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH8.bit.CFG_IMAGE_HEIGHT_OUT_CH8 = res0->dma2d_img_cfg.img_width;
	}else if(channel_res1 == dma_2d_ch9) {
		ch_img_out_len_reg_res1 = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.all;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_WIDTH_IN_CH9 = res0->dma2d_img_cfg.img_height;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_WIDTH_OUT_CH9 = res0->dma2d_img_cfg.img_height;

		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH9.bit.CFG_IMAGE_HEIGHT_IN_CH9 = res0->dma2d_img_cfg.img_width;
		IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH9.bit.CFG_IMAGE_HEIGHT_OUT_CH9 = res0->dma2d_img_cfg.img_width;
	}else
		return CSK_DRIVER_ERROR_PARAMETER;

    if (img_in_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
        *ch_img_in_len_reg_res1 = DMA2D_CH_MAX_BLOCK_LENGTH;
    } else {
        *ch_img_in_len_reg_res1 = res0->dma2d_img_cfg.img_height * 3 * 8 / 4;
    }

    uint32_t img_out_len_res1 = 0;
	if (dma2d_ch_info[res1->dma2d_init.dma_ch].status == dma2d_status_config_img) {
		img_out_len_res1 = res0->dma2d_img_cfg.img_height * dma2d_ch_info[res1->dma2d_init.dma_ch].block_out_size * 8 / 4;

		if(img_out_len_res1 > DMA2D_CH_MAX_BLOCK_LENGTH) {
			*ch_img_out_len_reg_res0 = DMA2D_CH_MAX_BLOCK_LENGTH;
		} else {
			*ch_img_out_len_reg_res1 = img_out_len_res1;
		}
	}

    // Configure mode
    dma2d_ch_info[res0->dma2d_init.dma_ch].mode = address_mode_normal;
    dma2d_ch_info[res0->dma2d_init.dma_ch].status = dma2d_status_busy;

    dma2d_ch_info[res1->dma2d_init.dma_ch].mode = address_mode_normal;
    dma2d_ch_info[res1->dma2d_init.dma_ch].status = dma2d_status_busy;

    // Enable finish interrupt
    IP_DMA2D->REG_DMA_IMAGE_INT_EN.bit.CFG_IMAGE_BLOCK_FINISH_INT_EN = 0x1;

    // Enable
    if (!(*ch_ctrl_res0 & 0x1)){
        *ch_ctrl_res0 |= 0x1;
    }

    if (!(*ch_ctrl_res1 & 0x1)){
        *ch_ctrl_res1 |= 0x1;
    }

    // Only start res0
    *ch_ctrl_res0 |= 0x2;

	return 0;
}




int32_t
DMA2D_Start_PiPo(csk_dma2d_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t img_in_len, uint32_t img_out_len) {
    uint8_t channel = ch;

    // Only in configure status can start pipo transmits
    if(dma2d_ch_info[channel].status != dma2d_status_config && dma2d_ch_info[channel].status != dma2d_status_config_img) {
        return CSK_DMA2D_STATUS_ERROR;
    }

    // Source0 and destination0 buffer must not be NULL pointer
    if (src0 == NULL || dst0 == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    
    // Source1 and destination1 buffer must not be NULL pointer
    /*if (src1 == NULL || dst1 == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }*/
    
    if (img_in_len > DMA2D_CH_MAX_BLOCK_LENGTH || img_out_len > DMA2D_CH_MAX_BLOCK_LENGTH) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    
    // Get channel control register
    uint32_t* ch_ctrl = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    ch_ctrl += channel;

    //Get channel length register
    uint32_t* ch_img_in_len_reg = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all;
    ch_img_in_len_reg += channel;

    uint32_t* ch_img_out_len = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
    if(channel == dma_2d_ch6)
        ch_img_out_len = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH6.all;
    else if(channel == dma_2d_ch7)
        ch_img_out_len = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH7.all;
    else if(channel == dma_2d_ch8)
        ch_img_out_len = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH8.all;
    else if(channel == dma_2d_ch9)
        ch_img_out_len = (uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_OUT_BLOCK_LEN_CH9.all;
    else
        return CSK_DRIVER_ERROR_PARAMETER;   

    uint32_t* src_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* src_address1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR1_CH0.all;
    uint32_t* dst_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;
    uint32_t* dst_address1 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR1_CH0.all;

   // Get channel source and destination register
    {
        if (channel >= dma_2d_ch6) {
            // Get channel source and destination register
            src_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH6.all;
            src_address1 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR1_CH6.all;
            dst_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH6.all;
            dst_address1 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR1_CH6.all;

            src_address0 += (channel - dma_2d_ch6) * 4;
            src_address1 += (channel - dma_2d_ch6) * 4;
            dst_address0 += (channel - dma_2d_ch6) * 4;
            dst_address1 += (channel - dma_2d_ch6) * 4;
        } else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // Configure source and destination
    *src_address0 = (uint32_t)src0;
    *dst_address0 = (uint32_t)dst0;

    if (src1) {
        *src_address1 = (uint32_t)src1;
    }

    if (dst1) {
        *dst_address1 = (uint32_t)dst1;
    }

    // Configure length
    dma2d_ch_info[channel].length = img_in_len;
    dma2d_ch_info[channel].xfer_length = 0;
    *ch_img_in_len_reg = img_in_len;
    *ch_img_out_len = img_out_len;

    // Configure mode
    dma2d_ch_info[channel].mode = address_mode_pipo;
    dma2d_ch_info[channel].status = dma2d_status_busy;

    // Enable finish interrupt
    IP_DMA2D->REG_DMA_IMAGE_INT_EN.bit.CFG_IMAGE_BLOCK_FINISH_INT_EN = 0x1;

    // Enable
    if (!(*ch_ctrl & 0x1)){
        *ch_ctrl |= 0x1;
    }

    // Start
    if (dma_2d_ch6 > dma2d_ch_info[channel].trigger_ch || dma_2d_ch9 < dma2d_ch_info[channel].trigger_ch)
    {
        *ch_ctrl |= 0x2;
    }

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_GetCnt(csk_dma2d_ch_t ch, uint32_t *sample_len) {
	uint8_t channel = ch;
	// Current channel have stopped
	if (dma2d_ch_info[channel].status == dma2d_status_config) {
		// Have transmit complete
		if(dma2d_ch_info[channel].xfer_length == dma2d_ch_info[channel].length) {
			*sample_len = dma2d_ch_info[channel].length;
		}
		// Have residue size
		else {
			uint16_t residue_len = 0;

			// Block length
			uint32_t *channel_length = (uint32_t *)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all + channel;

			// Choose remain length register
            IP_DMA2D->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = DMA2D_DIAG_SEL_REMAIN_CNT_MODE;
            IP_DMA2D->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

            residue_len = IP_DMA2D->REG_DMA_DIAG_RPT.all & DMA2D_CH_MAX_BLOCK_LENGTH_MASK;

            if (*channel_length < residue_len) {
                return CSK_DMA2D_UNKNOWN_ERROR;
            }
            *sample_len = dma2d_ch_info[channel].xfer_length + (*channel_length - residue_len);
		}
	}

    // Current channel still running
    else if (dma2d_ch_info[channel].status == dma2d_status_busy){
        *sample_len = dma2d_ch_info[channel].xfer_length;
    }

    else {
        return CSK_DMA2D_STATUS_ERROR;
    }
	return CSK_DRIVER_OK;
}


int32_t
DMA2D_Initialize(void) {
    if (DMA2D_GLB_FLAG == 0){

        // Rest DMA2D module
        IP_AP_CFG->REG_SW_RESET.bit.DMAC_GP_RESET = 1;

        memset(dma2d_ch_info, 0, sizeof(dma2d_ch_info));

        __HAL_CRM_GPDMA_CLK_ENABLE();

        // Clear DMA2D interrupt
        IP_DMA2D->REG_DMA_IMAGE_INT_CLR.bit.CFG_IMAGE_BLOCK_FINISH_CLR = 0xF;
        IP_DMA2D->REG_DMA_IMAGE_INT_CLR.bit.CFG_IMAGE_HALF_BLOCK_FINISH_CLR = 0xF;

        // Clear DMA2D error
        IP_DMA2D->REG_DMA_INT_CLR.bit.CFG_DMA_AHB_ERR_CLR = 0x1;

        // Disable DMA2D interrupt
        IP_DMA2D->REG_DMA_IMAGE_INT_EN.bit.CFG_IMAGE_BLOCK_FINISH_INT_EN = 0x0;
        IP_DMA2D->REG_DMA_IMAGE_INT_EN.bit.CFG_IMAGE_HALF_BLOCK_FINISH_INT_EN = 0x0;

        // Clear DMA2D config
        IP_DMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x3C0;

        // Register DMA2D global interrupt
        register_ISR(IRQ_DMAC_GP_IMG_VECTOR, DMA2D_IRQ_Handler, NULL);
        enable_IRQ(IRQ_DMAC_GP_IMG_VECTOR);

        uint8_t i = 0;
        for (i = 6; i < CSK_DMA2D_MAX_CHANNEL_NUM; i++) {
            dma2d_ch_info[i].status = dma2d_status_init;
        }
    }

    DMA2D_GLB_FLAG++;

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_Image_Config_Extend(csk_dma2d_ch_t ch, csk_dma_2d_image_cfg_t* img_cfg) {
	const uint32_t CHANNEL_MIN = dma_2d_ch6;

	const uint32_t CHANNEL_MAX = dma_2d_ch9;
	uint32_t c_plane_num = 2;
	// Validate channel range
	if(ch < CHANNEL_MIN || ch > CHANNEL_MAX) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	// Validate channel status
	if(dma2d_ch_info[ch].status != dma2d_status_config) {
		return CSK_DRIVER_ERROR;
	}

//	// Validate image dimensions
//	if(((img_cfg->img_width - img_cfg->start_col) % 8 != 0) || ((img_cfg->img_height - img_cfg->start_row) % 8 != 0)) {
//		return CSK_DRIVER_ERROR_PARAMETER;
//	}

	// Initialize block size and mode
	dma2d_ch_info[ch].block_out_size = 1;
	dma2d_ch_info[ch].block_in_size = 1;
	dma2d_ch_info[ch].mode_2d = csk_jpeg_enc_bypass;

    //*********************************************Jepg encode and decode
    // Clear Encode and Decode parameter
//	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS |= (0x1 << (ch - dma_2d_ch6)); // bypass
//	IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS |= (0x1 << (ch - dma_2d_ch6)); // bypass
//	IP_DMA2D->REG_DMA_DEC_OUT2D_BYPASS.bit.CFG_DEC_OUT2D_BYPASS |= (0x1 << (ch - dma_2d_ch6)); // bypass
//	IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS |= (0x1 << (ch - dma_2d_ch6)); // bypass
//	IP_DMA2D->REG_DMA_DEC_IN2D_BYPASS.bit.CFG_DEC_IN2D_BYPASS |= (0x1 << (ch - dma_2d_ch6)); // bypass

	// Configure JPEG encode and decode
	if (csk_image_format_yuv444 == img_cfg->img_input_format)
		c_plane_num = 0x1;
	else if (csk_image_format_yuv420 == img_cfg->img_input_format)
		c_plane_num = 0x4;
	else
		c_plane_num = 0x2;
	uint32_t img_jpeg_2d_type = img_cfg->img_jpeg_2d_type;
	switch (img_jpeg_2d_type) {
		case csk_jpeg_enc_bypass:
			break;
		case csk_jpeg_enc_unpack: // YUV -->>   YYYYY..../UUUUUUU..../VVVVV....
		{
			IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
			IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL &= ~(0x1 << (ch - dma_2d_ch6));
			IP_DMA2D->REG_DMA_ENC_OUT2D_BYPASS.bit.CFG_ENC_OUT2D_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
            if (dma_2d_ch6 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_BYPASS_CH6  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM1_CH6 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM2_CH6 = 0x1;
			}
			else if (dma_2d_ch7 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_BYPASS_CH7  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7  = 0x3;
			    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM1_CH7 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM2_CH7 = 0x1;
			}
			else if (dma_2d_ch8 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_BYPASS_CH8  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM1_CH8 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM2_CH8 = 0x1;
			}
			else
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0x1;
			}

			dma2d_ch_info[ch].mode_2d = csk_jpeg_enc_unpack;
		}
			break;
		case csk_jpeg_enc_2d_transfer:	// YYY.../UUU.../VVV... -->> Y*64/ U*64/ V*64 ....
		{
			IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL &= ~(0x1 << (ch - dma_2d_ch6)); // enable enc 2daddr
			IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
            if (dma_2d_ch6 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_BYPASS_CH6  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = 0x8;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = 0x2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM1_CH6 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM2_CH6 = 0x1;
			}
			else if (dma_2d_ch7 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_BYPASS_CH7  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = 0x8;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = 0x2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7  = 0x3;
			    IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM1_CH7 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM2_CH7 = 0x1;
			}
			else if (dma_2d_ch8 == ch)
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_BYPASS_CH8  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8 = 0x8;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8 = 0x2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM1_CH8 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM2_CH8 = 0x1;
			}
			else
			{
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_BYPASS_CH9  = 0x0;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = 0x8;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 0x2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9  = 0x3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = c_plane_num;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 0x1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 0x1;
			}
			dma2d_ch_info[ch].mode_2d = csk_jpeg_enc_2d_transfer;
		}
			break;
	    case csk_jpeg_dec_pack: // YYYYY..../UUUUUUU..../VVVVV.... -->>  YUV
	    {
	    	IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL |= (0x1 << (ch - dma_2d_ch6)); // enable dec 2daddr

	    	dma2d_ch_info[ch].mode_2d = csk_jpeg_dec_pack;
	    }
	    	break;
	    case csk_jpeg_dec_2d_transfer: // Y*64/ U*64/V*64 ....  -->>  YYY.../UUU.../VVV...
	    {
	    	IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL |= (0x1 << (ch - dma_2d_ch6)); // enable dec 2daddr

	    	dma2d_ch_info[ch].mode_2d = csk_jpeg_dec_2d_transfer;
	    }
	        break;
	    default:
	        // Error
	        break;
	}

	//*********************************************Input Format
	// Set Input Format
	uint32_t img_input_format_transfer = img_cfg->img_input_format;
	switch(img_input_format_transfer) {
		case csk_image_format_yuv422:
		{
			// Function to configure YUV422 input
			// EXCEL ERROR
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << ((ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((0x0) << ((ch - dma_2d_ch6) * 2));  //0: yuv422  1: yuv420  2:yuv422

			// Input YUV422 Format
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << (16 + (ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((img_cfg->img_yuv422_format & 0x3) << (16 + (ch - dma_2d_ch6) * 2));

			// Input YUV422
			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_YUV422_BYTES_SIZE;
		}
			break;
	    case csk_image_format_yuv444:
	    {
			// Function to configure YUV444 input
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << ((ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((0x2)  << ((ch - dma_2d_ch6) * 2));

			// Input YUV444
			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_YUV444_BYTES_SIZE;
	    }
	    	break;
	    case csk_image_format_yuv420:
	    {
	    	IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << ((ch - dma_2d_ch6) * 2));
	    	IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((0x1) << ((ch - dma_2d_ch6) * 2));	//0: yuv422  1: yuv420  2:yuv422
	    	IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << (8 + (ch - dma_2d_ch6) * 2));
	    	IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((img_cfg->img_yuv420_format & 0x3) << (8 + (ch - dma_2d_ch6) * 2));

	    	// Input YUV420
			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_YUV420_BYTES_SIZE;
	    }
	    	break;
	    case csk_image_format_xbgr:
			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

	    	// Input xbgr
			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_XBGR_BYTES_SIZE;
			break;
	    case csk_image_format_xrgb:
			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_XRGB_BYTES_SIZE;
			break;
	    case csk_image_format_argb:
			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_ARGB_BYTES_SIZE;
	    	break;
	    case csk_image_format_abgr:
			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

			dma2d_ch_info[ch].block_in_size = DMA2D_IN_FORMAT_ABGR_BYTES_SIZE;
	    	break;
	    default:
	    	break;
	}

	//*********************************************Format transfer
	// Disable YUV422 and RGB output by default
//	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS |= (0x1 << (ch - dma_2d_ch6));
//	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS |= (0x1 << (ch - dma_2d_ch6));

	// Set output format
	uint32_t img_output_format_transfer = img_cfg->img_output_fromat_transfer;
	switch(img_output_format_transfer) {
		case csk_image_format_transfer_bypass:
			dma2d_ch_info[ch].block_out_size = dma2d_ch_info[ch].block_in_size;
			break;
		case csk_image_format_transfer_yuv422_argb:
		case csk_image_format_transfer_yuv444_argb:
		case csk_image_format_transfer_yuv420_argb:
			// Enable ARGB output
			IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
			IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE &= ~(0x1 << (ch - dma_2d_ch6));

			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

			// Output ARGB
			dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_ARGB_BYTES_SIZE;
			break;
		case csk_image_format_transfer_yuv422_abgr:
		case csk_image_format_transfer_yuv444_abgr:
			// Enable ABGR output
			IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
			IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE &= ~(0x1 << (ch - dma_2d_ch6));

			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

			// Output ABGR
			dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_ARGB_BYTES_SIZE;
			break;
		case csk_image_format_transfer_yuv422_xrgb:
        case csk_image_format_transfer_yuv444_xrgb:
        case csk_image_format_transfer_yuv420_xrgb:
        	// Enable XRGB output
        	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (ch - dma_2d_ch6));

			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

        	// Output XRGB
			dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_XRGB_BYTES_SIZE;
			break;
        case csk_image_format_transfer_yuv422_xbgr:
        case csk_image_format_transfer_yuv444_xbgr:
        	// Enable XRGB output
        	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (ch - dma_2d_ch6));

			if(img_cfg->img_rgb888_format == csk_image_rgb888_format) // br exchange enable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (ch - dma_2d_ch6)));
			if(img_cfg->img_rgb888_format == csk_image_bgr888_format) // br exchange disable
				IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all &= ~(1 << (16 + (ch - dma_2d_ch6)));

        	// Output XRGB
			dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_XBGR_BYTES_SIZE;
        	break;
        case csk_image_format_transfer_yuv444_yuv422:
        case csk_image_format_transfer_yuv420_yuv422:
        	// Enable YUV422 output
            IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_YUV422_BYPASS &= ~(0x1 << (ch - dma_2d_ch6));

            // Output YUV422
            dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_YUV422_BYTES_SIZE;
            break;
        case csk_image_format_transfer_yuv422_y8:
        	IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (0x1 << (8 + (ch - dma_2d_ch6)));
        	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS &= ~(0x1 << (ch - dma_2d_ch6)); // bypass disable

			dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_Y8_BYTES_SIZE;
        	break;
        case csk_image_format_transfer_xbgr_y8:
        case csk_image_format_transfer_xrgb_y8:
        	IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (0x1 << (12 + (ch - dma_2d_ch6)));
//        	IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS &= ~(0x1 << (ch - dma_2d_ch6)); // bypass disable

        	dma2d_ch_info[ch].block_out_size = DMA2D_OUT_FORMAT_Y8_BYTES_SIZE;
        	break;
        case csk_image_format_transfer_yuv422_crop:
        case csk_image_format_transfer_xrgb_crop:
        case csk_image_format_transfer_xbgr_crop:
        case csk_image_fromat_transfer_abgr_crop:
        case csk_image_format_transfer_argb_crop:
        case csk_image_format_transfer_xrgb_crop_and_scale:
        case csk_image_format_transfer_argb_crop_and_scale:
        	IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS &= ~(0x1 << (ch - dma_2d_ch6)); // bypass disable

            if(ch == dma_2d_ch6) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = (img_cfg->end_col - img_cfg->start_col + 1) * dma2d_ch_info[ch].block_in_size / 4;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = (img_cfg->end_row - img_cfg->start_row + 1);

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = ((img_cfg->img_width - (img_cfg->end_col - img_cfg->start_col + 1)) * dma2d_ch_info[ch].block_in_size) + 4;

            } else if(ch == dma_2d_ch7) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = (img_cfg->end_col - img_cfg->start_col + 1) * dma2d_ch_info[ch].block_in_size / 4;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = (img_cfg->end_row - img_cfg->start_row + 1);

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = ((img_cfg->img_width - (img_cfg->end_col - img_cfg->start_col + 1)) * dma2d_ch_info[ch].block_in_size) + 4;

            } else if(ch == dma_2d_ch8) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8 = (img_cfg->end_col - img_cfg->start_col + 1) * dma2d_ch_info[ch].block_in_size / 4;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8 = (img_cfg->end_row - img_cfg->start_row + 1);

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = ((img_cfg->img_width - (img_cfg->end_col - img_cfg->start_col + 1)) * dma2d_ch_info[ch].block_in_size) + 4;

            } else {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = (img_cfg->end_col - img_cfg->start_col + 1) * dma2d_ch_info[ch].block_in_size / 4;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = (img_cfg->end_row - img_cfg->start_row + 1);

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = ((img_cfg->img_width - (img_cfg->end_col - img_cfg->start_col + 1)) * dma2d_ch_info[ch].block_in_size) + 4;
            }

            dma2d_ch_info[ch].block_out_size = dma2d_ch_info[ch].block_in_size;
            dma2d_ch_info[ch].is_cropped = DMA2D_IMAGE_CROPPED;

            dma2d_ch_info[ch].crop_offset = (img_cfg->start_row - 1) * (img_cfg->img_height) * dma2d_ch_info[ch].block_in_size + (img_cfg->start_col - 1) * dma2d_ch_info[ch].block_in_size ;

            if(img_cfg->img_output_fromat_transfer == csk_image_format_transfer_xrgb_crop_and_scale || img_cfg->img_output_fromat_transfer == csk_image_format_transfer_argb_crop_and_scale) {
            	dma2d_ch_info[ch].is_zoom_scale = DMA2D_IMAGER_ZOOM_SCALE;
            }
        	break;
        default:
        	break;
	}

	//*********************************************Zoom out
	// Configure image size and scaling
	volatile uint32_t* img_size_in = (volatile uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.all + (ch - dma_2d_ch6) * 2;

	volatile uint32_t* img_size_out = (volatile uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.all + (ch - dma_2d_ch6) * 2;

	if (img_cfg->img_zoom_scale == csk_image_zoom_scale_1_to_1) {
		if (img_cfg->start_col != 0 || img_cfg->start_row != 0 || img_cfg->end_col != 0 || img_cfg->end_row != 0) {
			*img_size_in = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
				                       (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

		} else {
	        *img_size_in = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                       (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		}
	} else if (img_cfg->img_zoom_scale == csk_image_zoom_scale_1_to_2) {
		if(img_cfg->start_col != 0 || img_cfg->start_row != 0 || img_cfg->end_col != 0 || img_cfg->end_row != 0) {
			*img_size_in = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
				                       (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		} else {
	        *img_size_in = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                       (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		}
		// TODO: Implement 1/2 scale
        if(img_cfg->img_input_format == csk_image_format_xbgr || img_cfg->img_input_format == csk_image_format_xrgb) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (ch - dma_2d_ch6));
        }

        if(img_cfg->img_input_format == csk_image_format_argb || img_cfg->img_input_format == csk_image_format_abgr) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE &= ~(0x1 << (ch - dma_2d_ch6));
        }


		IP_DMA2D->REG_DMA_ZOOM_MODE.all = (csk_image_zoom_scale_1_to_2 << ((ch - dma_2d_ch6) * 2));

		dma2d_ch_info[ch].block_in_size *= 4;
	} else if (img_cfg->img_zoom_scale == csk_image_zoom_scale_1_to_3) {
		if(img_cfg->start_col != 0 || img_cfg->start_row != 0 || img_cfg->end_col != 0 || img_cfg->end_row != 0) {
			*img_size_in = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
				                       (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		} else {
			// TODO: Implement 1/3 scale
	        *img_size_in = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                       (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		}
        if(img_cfg->img_input_format == csk_image_format_xbgr || img_cfg->img_input_format == csk_image_format_xrgb) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (ch - dma_2d_ch6));
        }

        if(img_cfg->img_input_format == csk_image_format_argb || img_cfg->img_input_format == csk_image_format_abgr) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE &= ~(0x1 << (ch - dma_2d_ch6));
        }


		IP_DMA2D->REG_DMA_ZOOM_MODE.all = (csk_image_zoom_scale_1_to_3 << ((ch - dma_2d_ch6) * 2));

		dma2d_ch_info[ch].block_in_size *= 9;
	} else if (img_cfg->img_zoom_scale == csk_image_zoom_scale_1_to_4) {
		if(img_cfg->start_col != 0 || img_cfg->start_row != 0 || img_cfg->end_col != 0 || img_cfg->end_row != 0) {
			*img_size_in = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
				                       (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->end_col - img_cfg->start_col + 1) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->end_row - img_cfg->start_row + 1) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		} else {
			// TODO: Implement 1/4 scale
	        *img_size_in = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                       (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

	        *img_size_out = ((img_cfg->img_width - img_cfg->start_col) & DMA2D_CH_IMG_MAX_WIDTH) |
	                        (((img_cfg->img_height - img_cfg->start_row) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
		}
        if(img_cfg->img_input_format == csk_image_format_xbgr || img_cfg->img_input_format == csk_image_format_xrgb) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (ch - dma_2d_ch6));
        }

        if(img_cfg->img_input_format == csk_image_format_argb || img_cfg->img_input_format == csk_image_format_abgr) {
        	IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE &= ~(0x1 << (ch - dma_2d_ch6));
        }


		IP_DMA2D->REG_DMA_ZOOM_MODE.all = (csk_image_zoom_scale_1_to_4 << ((ch - dma_2d_ch6) * 2));

		dma2d_ch_info[ch].block_in_size *= 16;
	}

	dma2d_ch_info[ch].status |= dma2d_status_config_img;

    return CSK_DRIVER_OK;
}

int32_t
DMA2D_Image_Rotate_Config_Extend(csk_dma_2d_rotate_cfg_t *res0, csk_dma_2d_rotate_cfg_t *res1) {
	const uint32_t CHANNEL_MIN = dma_2d_ch6;
	const uint32_t CHANNEL_MAX = dma_2d_ch9;

	if(res0->dma2d_init.dma_ch < CHANNEL_MIN || res0->dma2d_init.dma_ch > CHANNEL_MAX) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	if(res1->dma2d_init.dma_ch < CHANNEL_MIN || res1->dma2d_init.dma_ch > CHANNEL_MAX) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

	// Validate channel status
	if(dma2d_ch_info[res0->dma2d_init.dma_ch].status != dma2d_status_config) {
		return CSK_DRIVER_ERROR;
	}

	if(dma2d_ch_info[res1->dma2d_init.dma_ch].status != dma2d_status_config) {
		return CSK_DRIVER_ERROR;
	}

	// Initialize block size and mode
	dma2d_ch_info[res0->dma2d_init.dma_ch].block_out_size = 1;
	dma2d_ch_info[res0->dma2d_init.dma_ch].block_in_size = 1;
	dma2d_ch_info[res0->dma2d_init.dma_ch].mode_2d = csk_jpeg_enc_bypass;

	dma2d_ch_info[res1->dma2d_init.dma_ch].block_out_size = 1;
	dma2d_ch_info[res1->dma2d_init.dma_ch].block_in_size = 1;
	dma2d_ch_info[res1->dma2d_init.dma_ch].mode_2d = csk_jpeg_enc_bypass;

	// Configure JPEG encode and decode
	uint32_t img_jpeg_2d_type_res0 = res0->dma2d_img_cfg.img_jpeg_2d_type;
	switch (img_jpeg_2d_type_res0) {
		case csk_jpeg_enc_bypass:
			break;
		case csk_jpeg_enc_unpack: // YUV -->>   YYYYY..../UUUUUUU..../VVVVV....
		{
			IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS1.bit.CFG_YUV_UNPACK_BYPASS &= ~(0x1 << (res0->dma2d_init.dma_ch - dma_2d_ch6));
			IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL &= ~(0x1 << (res0->dma2d_init.dma_ch - dma_2d_ch6));

			dma2d_ch_info[res0->dma2d_init.dma_ch].mode_2d = csk_jpeg_enc_unpack;
		}
			break;
		case csk_jpeg_enc_2d_transfer:	// YYY.../UUU.../VVV... -->> Y*64/ U*64/ V*64 ....
		{
			IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL &= ~(0x1 << (res0->dma2d_init.dma_ch - dma_2d_ch6)); // enable enc 2daddr

			dma2d_ch_info[res0->dma2d_init.dma_ch].mode_2d = csk_jpeg_enc_2d_transfer;
		}
			break;
	    case csk_jpeg_dec_pack: // YYYYY..../UUUUUUU..../VVVVV.... -->>  YUV
	    {
	    	IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL |= (0x1 << (res0->dma2d_init.dma_ch - dma_2d_ch6)); // enable dec 2daddr

	    	dma2d_ch_info[res0->dma2d_init.dma_ch].mode_2d = csk_jpeg_dec_pack;
	    }
	    	break;
	    case csk_jpeg_dec_2d_transfer: // Y*64/ U*64/V*64 ....  -->>  YYY.../UUU.../VVV...
	    {
	    	IP_DMA2D->REG_DMA_IMAGE_ENC_DEC_CTRL.bit.CFG_ENC_DEC_SEL |= (0x1 << (res0->dma2d_init.dma_ch - dma_2d_ch6)); // enable dec 2daddr

	    	dma2d_ch_info[res0->dma2d_init.dma_ch].mode_2d = csk_jpeg_dec_2d_transfer;
	    }
	    	break;
	    default:
	    	//Error
	    	break;
	}

	//*********************************************Input Format
	// Set Input Format
	uint32_t img_input_format_transfer_res0 = res0->dma2d_img_cfg.img_input_format;
	switch(img_input_format_transfer_res0) {
		case csk_image_format_yuv422:
		{
			// Function to configure YUV422 input
			// EXCEL ERROR
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << ((res0->dma2d_init.dma_ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((0x0) << ((res0->dma2d_init.dma_ch - dma_2d_ch6) * 2));  //0: yuv422  1: yuv420  2:yuv422

			// Input YUV422 Format
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << (16 + (res0->dma2d_init.dma_ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((res0->dma2d_img_cfg.img_yuv422_format & 0x3) << (16 + (res0->dma2d_init.dma_ch - dma_2d_ch6) * 2));

			// Input YUV422
			dma2d_ch_info[res0->dma2d_init.dma_ch].block_in_size = DMA2D_IN_FORMAT_YUV422_BYTES_SIZE;

			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << ((res1->dma2d_init.dma_ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((0x0) << ((res1->dma2d_init.dma_ch - dma_2d_ch6) * 2));  //0: yuv422  1: yuv420  2:yuv422

			// Input YUV422 Format
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all &= ~(0x3 << (16 + (res1->dma2d_init.dma_ch - dma_2d_ch6) * 2));
			IP_DMA2D->REG_DMA_IMAGE_FORMAT.all |= ((res1->dma2d_img_cfg.img_yuv422_format & 0x3) << (16 + (res1->dma2d_init.dma_ch - dma_2d_ch6) * 2));

			// Input YUV422
			dma2d_ch_info[res1->dma2d_init.dma_ch].block_in_size = DMA2D_IN_FORMAT_YUV422_BYTES_SIZE;
		}
		break;
		default:
			break;
	}

	//*********************************************Format transfer
	// Format transfer
	uint32_t img_output_format_transfer_res0 = res0->dma2d_img_cfg.img_output_fromat_transfer;
	switch (img_output_format_transfer_res0) {
		case csk_image_format_transfer_bypass:
			break;
		case csk_image_format_transfer_yuv422_cw_90:
		case csk_image_format_transfer_yuv422_ccw_90:
		case csk_image_fromat_transfer_yuv422_cw90_to_rgb:
		case csk_image_fromat_transfer_yuv422_ccw90_to_rgb:
			IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= ((res0->dma2d_img_cfg.image_yuv422_rotate_mode) << ((res0->dma2d_init.dma_ch - dma_2d_ch6) * 2));

            if(res0->dma2d_init.dma_ch == dma_2d_ch6) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = res0->dma2d_img_cfg.img_height;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM1_CH6 = res0->dma2d_img_cfg.img_width / 2;;

				if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_clockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = -(res0->dma2d_img_cfg.img_width * 2);
				} else if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_counterclockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = (res0->dma2d_img_cfg.img_width * 2);
				}
            } else if(res0->dma2d_init.dma_ch == dma_2d_ch7) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = res0->dma2d_img_cfg.img_height;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM1_CH7 = res0->dma2d_img_cfg.img_width / 2;;

				if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_clockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = -(res0->dma2d_img_cfg.img_width * 2);
				} else if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_counterclockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = (res0->dma2d_img_cfg.img_width * 2);
				}
            } else if(res0->dma2d_init.dma_ch == dma_2d_ch8) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8 = res0->dma2d_img_cfg.img_height;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8 = 2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM1_CH8 = res0->dma2d_img_cfg.img_width / 2;

				if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_clockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = -(res0->dma2d_img_cfg.img_width * 2);
				} else if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_counterclockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = (res0->dma2d_img_cfg.img_width * 2);
				}
            } else {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = res0->dma2d_img_cfg.img_height;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 2;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = res0->dma2d_img_cfg.img_width / 2;

				if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_clockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = -(res0->dma2d_img_cfg.img_width * 2);
				} else if(res0->dma2d_img_cfg.image_yuv422_rotate_mode == csk_image_yuv422_counterclockwise_rotate) {
					IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = (res0->dma2d_img_cfg.img_width * 2);
				}
            }

            dma2d_ch_info[res0->dma2d_init.dma_ch].block_out_size = DMA2D_IN_FORMAT_YUV422_BYTES_SIZE;
            IP_DMA2D->REG_DMA_ENC_IN2D_BYPASS.bit.CFG_ENC_IN2D_BYPASS &= ~(0x1 << (res1->dma2d_init.dma_ch - dma_2d_ch6)); // bypass disable

            if(res1->dma2d_init.dma_ch == dma_2d_ch6) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_WNUM_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH6.bit.CFG_D2_ADDR_HNUM_CH6 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM_CH6 = 3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH6.bit.CFG_D2_ADDR_BLK_NUM0_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM1_CH6 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH6.bit.CFG_D2_ADDR_BLK_NUM2_CH6 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH6.bit.CFG_D2_ADDR_STEP_S_CH6 = 0;

            } else if(res1->dma2d_init.dma_ch == dma_2d_ch7) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_WNUM_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH7.bit.CFG_D2_ADDR_HNUM_CH7 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM_CH7 = 3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH7.bit.CFG_D2_ADDR_BLK_NUM0_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM1_CH7 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH7.bit.CFG_D2_ADDR_BLK_NUM2_CH7 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH7.bit.CFG_D2_ADDR_STEP_S_CH7 = 0;
            } else if(res1->dma2d_init.dma_ch == dma_2d_ch8) {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_WNUM_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH8.bit.CFG_D2_ADDR_HNUM_CH8 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM_CH8 = 3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH8.bit.CFG_D2_ADDR_BLK_NUM0_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM1_CH8 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH8.bit.CFG_D2_ADDR_BLK_NUM2_CH8 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH8.bit.CFG_D2_ADDR_STEP_S_CH8 = 0;

            } else {
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all &= ~(0x1 << 30);
            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.all |= (0x1 << 29);

            	IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_WNUM_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL0_CH9.bit.CFG_D2_ADDR_HNUM_CH9 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM_CH9 = 3;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL3_CH9.bit.CFG_D2_ADDR_BLK_NUM0_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM1_CH9 = 1;
				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL4_CH9.bit.CFG_D2_ADDR_BLK_NUM2_CH9 = 1;

				IP_DMA2D->REG_DMA_IMAGE_D2_ADDR_CTRL1_CH9.bit.CFG_D2_ADDR_STEP_S_CH9 = 0;
            }

            dma2d_ch_info[res1->dma2d_init.dma_ch].block_out_size = DMA2D_IN_FORMAT_YUV422_BYTES_SIZE;

            IP_DMA2D->REG_DMA_CH_TRIGGER_CTRL.bit.CFG_CH_TRIGGERED_EN |= 0x1 << (res1->dma2d_init.dma_ch - dma_2d_ch6);
            IP_DMA2D->REG_DMA_CH_TRIGGER_CTRL.all |= (res0->dma2d_init.dma_ch - dma_2d_ch6) << ((res1->dma2d_init.dma_ch - dma_2d_ch6) * 2);
            IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= 0x1 << (20 + (res1->dma2d_init.dma_ch - dma_2d_ch6));

            if(res0->dma2d_img_cfg.img_output_fromat_transfer == csk_image_fromat_transfer_yuv422_cw90_to_rgb || res0->dma2d_img_cfg.img_output_fromat_transfer == csk_image_fromat_transfer_yuv422_ccw90_to_rgb)
            {
    			IP_DMA2D->REG_DMA_IMAGE_PROC_BYPASS0.bit.CFG_YUV_TO_RGB_BYPASS &= ~(0x1 << (res1->dma2d_init.dma_ch - dma_2d_ch6));
    			IP_DMA2D->REG_DMA_RGB_MODE.bit.CFG_RGB_MODE |= (0x1 << (res1->dma2d_init.dma_ch - dma_2d_ch6));
    			IP_DMA2D->REG_DMA_IMAGE_FEATURE_CTRL.all |= (1 << (16 + (res1->dma2d_init.dma_ch - dma_2d_ch6)));

    			dma2d_ch_info[res1->dma2d_init.dma_ch].block_out_size = DMA2D_IN_FORMAT_XRGB_BYTES_SIZE;
            }
			break;
		default:
			break;
	}

	//*********************************************Zoom out and scale
	// Zoom out and scale
	volatile uint32_t* img_size_in_res0 = (volatile uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_IN_CH6.all + (res0->dma2d_init.dma_ch - dma_2d_ch6) * 2;
	volatile uint32_t* img_size_out_res0 = (volatile uint32_t*)&IP_DMA2D->REG_DMA_IMAGE_SIZE_CONFIG_OUT_CH6.all + (res0->dma2d_init.dma_ch - dma_2d_ch6) * 2;

	if (res0->dma2d_img_cfg.img_zoom_scale == csk_image_zoom_scale_1_to_1) {
        *img_size_in_res0 = ((res0->dma2d_img_cfg.img_width) & DMA2D_CH_IMG_MAX_WIDTH) |
                       (((res0->dma2d_img_cfg.img_height) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);

        *img_size_out_res0 = ((res0->dma2d_img_cfg.img_width) & DMA2D_CH_IMG_MAX_WIDTH) |
                        (((res0->dma2d_img_cfg.img_height) & DMA2D_CH_IMG_MAX_HEIGHT) << 16);
	}

	dma2d_ch_info[res0->dma2d_init.dma_ch].status = dma2d_status_config_img;
	dma2d_ch_info[res1->dma2d_init.dma_ch].status = dma2d_status_config_img;

	return CSK_DRIVER_OK;
}



static void
DMA2D_IRQ_Handler(void){
    volatile uint32_t int_status = IP_DMA2D->REG_DMA_IMAGE_INT_STATUS.bit.IMAGE_BLOCK_FINISH_STATUS;

    // Clear interrupt pending
    IP_DMA2D->REG_DMA_IMAGE_INT_CLR.bit.CFG_IMAGE_BLOCK_FINISH_CLR = int_status;

    int_status = int_status << CSK_GPDMA_MAX_CHANNEL_NUM;

    // Get channel register
    uint32_t* channel_control = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all;
    uint32_t* channel_length = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all;
    uint32_t* src_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
    uint32_t* dst_address0 = (uint32_t*)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;

    csk_dma2d_ch_info_t* p_channel = NULL;

    uint8_t channel = 0;

    uint32_t event = 0;

    while(int_status) {
        // Corresponding interrupt trigger
        if (int_status & 0x1) {

            channel_control = (uint32_t*)&IP_DMA2D->REG_DMA_CH0_CTRL.all + channel;

            channel_length = (uint32_t*)&IP_DMA2D->REG_DMA_BLOCK_LEN_CH0.all + channel;

            // TODO This is a BUG for address fill error in EXCEL
            // Get channel source and destination register
            src_address0 = (uint32_t *)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH0.all;
            dst_address0 = (uint32_t *)&IP_DMA2D->REG_DMA_DST_ADDR0_CH0.all;

            if (channel >= dma_2d_ch6) {
                // Get channel source and destination register
                src_address0 = (uint32_t *)&IP_DMA2D->REG_DMA_SRC_ADDR0_CH6.all;
                dst_address0 = (uint32_t *)&IP_DMA2D->REG_DMA_DST_ADDR0_CH6.all;

                src_address0 += (channel - dma_2d_ch6) * 4;
                dst_address0 += (channel - dma_2d_ch6) * 4;
            }

            p_channel = (csk_dma2d_ch_info_t*)&dma2d_ch_info[channel];

            // **************** Normal mode
            if (p_channel->mode == address_mode_normal) {
                // Need transfer residue data
                if ((p_channel->length - p_channel->xfer_length) > DMA2D_CH_MAX_BLOCK_LENGTH){

                    p_channel->xfer_length += *channel_length;

                    // ****************Length
                    // Calculate residue size
                    if ((p_channel->length - p_channel->xfer_length) >= DMA2D_CH_MAX_BLOCK_LENGTH){
                        *channel_length = DMA2D_CH_MAX_BLOCK_LENGTH;
                    } else {
                        *channel_length = (p_channel->length - p_channel->xfer_length);
                    }

                    // ****************Source
                    // Increase mode
                    if (!(*channel_control & (0x1 << DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET))){
                        *src_address0 = *src_address0 + DMA2D_CH_MAX_BLOCK_LENGTH;
                    }

                    // ****************Destination
                    // Increase mode
                    if (!(*channel_control & (0x1 << DMA2D_CH_CTRL_DST_INC_MODE_OFFSET))){
                        *dst_address0 = *dst_address0 + DMA2D_CH_MAX_BLOCK_LENGTH;
                    }

                    // ****************Start
                    *channel_control |= 0x2;
                }

                // It's the last block
                else {
                    // Transmit complete
                    p_channel->xfer_length = p_channel->length;

                    event |= CSK_DMA2D_EVENT_TRANSFER_DONE;

                    // Change status to <config>
                    p_channel->status = dma2d_status_config;
                }
            }

            // **************** PIPO mode
            if (p_channel->mode == address_mode_pipo) {
                // Increase length
                p_channel->xfer_length += p_channel->length;

                IP_DMA2D->REG_DMA_DIAG_SEL.bit.CFG_DIAG_SEL = DMA2D_DIAG_SEL_TOG_FLAG_MODE;
                IP_DMA2D->REG_DMA_DIAG_SEL.bit.CFG_DIAG_CH_SEL = channel;

                if ((IP_DMA2D->REG_DMA_DIAG_RPT.all & 0x300000) >> 20) {
                    event |= CSK_DMA2D_EVENT_PIPO1_DONE;
                } else {
                    event |= CSK_DMA2D_EVENT_PIPO0_DONE;
                }

                event |= CSK_DMA2D_EVENT_TRANSFER_DONE;
            }

            // Callback
            if (p_channel->cb_event != NULL && event != 0) {
                p_channel->cb_event(event, p_channel->workspace);
            }
        }

        int_status = int_status >> 1;

        // increase channel number
        channel++;
    }
}
