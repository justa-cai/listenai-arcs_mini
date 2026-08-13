#include <string.h>
#include <stdio.h>
#include "ClockManager.h"
#include "log_print.h"
#include "venusa_ap.h"

#include "Driver_DMA2D.h"

// DMA2D Control register
/*
 *  This group contains bitfield definitions for the DMA2D control register
 */

#define DMA2D_CH_CTRL_HS_SEL_OFFSET         28 ///< Handshake selection offset
#define DMA2D_CH_CTRL_ONLINE_D_DST_OFFSET   26 ///< Online double destination offset
#define DMA2D_CH_CTRL_QSPI_EN_OFFSE T       25 ///< Quad SPI enable offset (typo in original)
#define DMA2D_CH_CTRL_DVP_EN_OFFSET         24 ///< Display virtual channel enable offset
#define DMA2D_CH_CTRL_LINE_STR_OFFSET       23 ///< Line stride enable offset
#define DMA2D_CH_CTRL_CFG_SGEN_OFFSET       22 ///< Source gather engine configuration offset
#define DMA2D_CH_CTRL_CFG_DSEN_OFFSET       21 ///< Destination scatter engine configuration offset
#define DMA2D_CH_CTRL_CH_PRIO_OFFSET        19 ///< Channel priority offset
#define DMA2D_CH_CTRL_ONLINE_D_SRC_OFFSET   18 ///< Online double source offset
#define DMA2D_CH_CTRL_FLOW_CTRL_OFFSET      17 ///< Flow control offset
#define DMA2D_CH_CTRL_DST_BURST_OFFSET      15 ///< Destination burst length offset
#define DMA2D_CH_CTRL_SRC_BURST_OFFSET      13 ///< Source burst length offset
#define DMA2D_CH_CTRL_DST_INC_MODE_OFFSET   10 ///< Destination addressing mode offset
#define DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET   8  ///< Source addressing mode offset
#define DMA2D_CH_CTRL_AHB_LOCK_OFFSET       7  ///< AHB lock enable offset
#define DMA2D_CH_CTRL_DST_BASE_UNIT_OFFSET  5 ///< Destination base unit offset
#define DMA2D_CH_CTRL_SRC_BASE_UNIT_OFFSET  3 ///< Source base unit offset
#define DMA2D_CH_CTRL_TFR_MODE_OFFSET       1  ///< Transfer mode offset
#define DMA2D_CH_CTRL_CH_EN_OFFSET          0  ///< Channel enable offset


// DMA2D ahb max len register
/*
 *  This group contains bitfield definitions for AHB maximum length registers
 */

#define DMA2D_AHB_MAXLEN_WR_OFFSET          4 ///< Write maximum length offset
#define DMA2D_AHB_MAXLEN_RD_OFFSET          0 ///< Read maximum length offset

// DMA2D triggered register
/*
 *  This group contains bitfield definitions for triggered operation registers
 */

#define DMA2D_TRIGGERED_SRC_OFFSET          4 ///< Trigger source offset
#define DMA2D_TRIGGERED_EN_OFFSET           0 ///< Trigger enable offset


// DMA2D image format register
/*
 *  This group contains bitfield definitions for image format configuration
 */
#define DMA2D_IMAGE_RGB_FORMAT_OUT_OFFSET   13 ///< Output RGB format offset
#define DMA2D_IMAGE_RGB_FORMAT_IN_OFFSET    12 ///< Input RGB format offset
#define DMA2D_IMAGE_YUV422_FORMAT_OUT_OFFSET 10 ///< Output YUV422 format offset
#define DMA2D_IMAGE_YUV422_FORMAT_IN_OFFSET 8  ///< Input YUV422 format offset
#define DMA2D_IMAGE_OUT_FORMAT_OFFSET       4  ///< Output format offset
#define DMA2D_IMAGE_IN_FORMAT_OFFSET        0  ///< Input format offset


// DMA2D image jpeg register
/*
 *  This group contains bitfield definitions for JPEG encoding/decoding
 */
#define DMA2D_JPEG_DATA_TYPE_OFFSET         2 ///< JPEG data type offset
#define DMA2D_JPEG_ENC_EN_OFFSET            1 ///< JPEG encoder enable offset
#define DMA2D_JPEG_DEC_EN_OFFSET            0 ///< JPEG decoder enable offset


// DMA2D image rotation register
/*
 *  This group contains bitfield definitions for image rotation operations
 */
#define DMA2D_ROTA_TILE_MODE_OFFSET         4 ///< Tiled rotation mode offset
#define DMA2D_ROTA_MODE_OFFSET              1 ///< Rotation mode offset
#define DMA2D_ROTA_EN_OFFSET                0 ///< Rotation enable offset


// DMA2D image size register
/*
 *  This group contains bitfield definitions for image dimensions
 */
#define DMA2D_SIZE_H_OFFSET                 16 ///< Image height offset
#define DMA2D_SIZE_W_OFFSET                 0  ///< Image width offset

// DMA2D image line stride register
/*
 *  This group contains bitfield definitions for line stride configuration
 */
#define DMA2D_LINE_STR_OUT_OFFSET           16 ///< Output line stride offset
#define DMA2D_LINE_STR_IN_OFFSET            0  ///< Input line stride offset

// DMA2D image mirror register
/*
 *  This group contains bitfield definitions for image mirroring operations
 */
#define DMA2D_MIRROR_MODE_OFFSET            1 ///< Mirror mode offset
#define DMA2D_MIRROR_EN_OFFSET              0 ///< Mirror enable offset

// DMA2D image copy register
/*
 *  This group contains bitfield definitions for image copy operations
 */
#define DMA2D_COPY_EN_OFFSET                24 ///< Copy enable offset
#define DMA2D_COPY_ROW_OFFSET               12 ///< Copy row offset
#define DMA2D_COPY_COL_OFFSET               0  ///< Copy column offset

// DMA2D image scaler register
/*
 *  This group contains bitfield definitions for image scaling operations
 */
#define DMA2D_SCALER_COL_OVERLAP_OFFSET     15 ///< two col data need overlap offset
#define DMA2D_SCALER_LAST_BAND_OFFSET       14 ///< Last band offset
#define DMA2D_SCALER_LAST_TILE_OFFSET       13 ///< Last tile offset
#define DMA2D_SCALER_MERGE_VIRT_MODE_OFFSET 9 ///< Virtual merge mode offset
#define DMA2D_SCALER_MERGE_HOR_MODE_OFFSET  5 ///< Horizontal merge mode offset
#define DMA2D_SCALER_MERGE_FLAG_OFFSET      4 ///< Merge flag offset
#define DMA2D_SCALER_FLAG_OFFSET            3 ///< Scaling flag offset
#define DMA2D_SCALER_VERT_DIRT_OFFSET       2 ///< Vertical direction offset
#define DMA2D_SCALER_HOR_DIRT_OFFSET        1 ///< Horizontal direction offset
#define DMA2D_SCALER_EN_OFFSET              0 ///< Scaling enable offset

#define DMA2D_SCALER_PHASESTEP_VIRT_OFFSET  16 ///< Virtual phase step offset
#define DMA2D_SCALER_PHASESTEP_HOR_OFFSET   0  ///< Horizontal phase step offset

#define DMA2D_SCALER_STARTPHASE_VIRT_OFFSET 16 ///< Virtual start phase offset
#define DMA2D_SCALER_STARTPHASE_HOR_OFFSET  0  ///< Horizontal start phase offset

#define DMA2D_SCALER_MAX_WIDTH_HALF         128///< Scaler handle max width half(pixel/2)

// DMA2D DIAGRAM CONFIGURE
/*
 *  This group contains macro definitions for diagram configuration options
 */
#define DMA2D_DIAG_SEL_TOG_FLAG_MODE        0 ///< Toggle flag mode
#define DMA2D_DIAG_SEL_REMAIN_CNT_MODE      0 ///< Remaining count mode
#define DMA2D_DIAG_SEL_SRC_ADDRESS_MODE     4 ///< Source address mode
#define DMA2D_DIAG_SEL_DST_ADDRESS_MODE     5 ///< Destination address mode


// Image format byte sizes
/*
 *  This group defines byte sizes for different image formats
 */
#define DMA2D_IN_FORMAT_YUV444_BYTES_SIZE   4 ///< YUV444 bytes per pixel
#define DMA2D_IN_FORMAT_ARGB_BYTES_SIZE       4 ///< ARGB bytes per pixel
#define DMA2D_IN_FORMAT_ABGR_BYTES_SIZE       4 ///< ABGR bytes per pixel
#define DMA2D_IN_FORMAT_XRGB_BYTES_SIZE       3 ///< XRGB bytes per pixel
#define DMA2D_IN_FORMAT_XBGR_BYTES_SIZE       3 ///< XBGR bytes per pixel
#define DMA2D_IN_FORMAT_YUV422_BYTES_SIZE   2 ///< YUV422 bytes per pixel
#define DMA2D_IN_FORMAT_YUV420_BYTES_SIZE   2 ///< YUV420 bytes per pixel

// Image crop flags
/*
 *  This group defines flags indicating image cropping status
 */
#define DMA2D_IMAGE_CROPPED                  1 ///< Image is cropped
#define DMA2D_IMAGE_NOT_CROPPED              0 ///< Image is not cropped

// Image zoom flags
/*
 *  This group defines flags indicating image zoom status
 */
#define DMA2D_IMAGER_ZOOM_SCALE             1 ///< Image is zoomed
#define DMA2D_IMAGER_NOT_ZOOM_SCALE         0 ///< Image is not zoomed


// DMA2D MAX BLOCK
/*
 *  This group defines maximum block length limits for DMA transfers
 */
#define DMA2D_CH_MAX_BLOCK_LENGTH           0xFFFFFF ///< Maximum block length (24-bit)
#define DMA2D_CH_MAX_BLOCK_LENGTH_MASK      0xFFFFFF ///< Block length mask


#define DMA2D_CH_IMG_MAX_WIDTH              0x7FF ///< Maximum image width (11-bit)
#define DMA2D_CH_IMG_MAX_HEIGHT             0x7FF ///< Maximum image height (11-bit)

typedef struct _csk_dma2d_ch_info {
    /** @brief Channel event callback function */
    CSK_DMA2D_SignalEvent_t cb_event;
    /** @brief Channel working memory space */
    void* workspace;
    /** @brief Channel current status */
    csk_dma2d_status_t status;
    /** @brief Total transfer length (normal mode) or buffer length (PiPo mode) */
    uint32_t length;
    /** @brief Currently transferred length */
    uint32_t xfer_length;
    /** @brief Total transfer length for one TRANSFER_DONE interrupt */
    uint32_t total_xfer_length;
    /** @brief Latest block transfer for one BLOCK_DONE interrupt */
    csk_dma2d_work_param_t last_blk;
    /** @brief JPEG codec mode for this channel */
    csk_jpeg_codec_mode_t mode_2d;
    /** @brief Input block size */
    uint8_t block_in_size;
    /** @brief Output block size */
    uint8_t block_out_size;
    /** @brief Source addressing mode */
    csk_inc_mode_t src_inc_mode;
    /** @brief Destination addressing mode */
    csk_inc_mode_t dst_inc_mode;
} csk_dma2d_ch_info_t;

/** @brief Array storing DMA2D channel information */
static csk_dma2d_ch_info_t dma2d_ch_info[CSK_DMA2D_MAX_CHANNEL_NUM] = {0};

/**
 * @brief Stop a DMA2D channel transfer
 *
 * @param chn Channel number to stop
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Stop(csk_dma2d_ch_t chn) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint32_t *ch_control = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;
    uint32_t *ch_irq_clr = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_CLR_00.all + chn;

    // Clear interrupt and config
    *ch_irq_clr |= 0x1F;
    IP_GPDMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR |= (0x1 << chn);

    // Delay
    __ASM volatile("nop");
    __ASM volatile("nop");
    __ASM volatile("nop");
    __ASM volatile("nop");
    __ASM volatile("nop");

    // Disable channel
    *ch_control &= ~(0x1 << DMA2D_CH_CTRL_CH_EN_OFFSET);

    dma2d_ch_info[chn].status = dma2d_status_config;
    
    return CSK_DRIVER_OK;
}

/**
 * @brief Configure a DMA2D channel
 *
 * @param res Initialization structure containing channel parameters
 * @param cb_event Event callback function pointer
 * @param workspace Working memory space pointer
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Config(csk_dma2d_init_t* res, CSK_DMA2D_SignalEvent_t cb_event, void* workspace) {
    if (NULL == res || dma_2d_ch0 > res->dma_ch || dma_2d_ch5 < res->dma_ch || tfr_mode_p2m > res->tfr_mode || tfr_mode_m2m < res->tfr_mode
        || dma2d_sample_unit_byte > res->src_basic_unit || dma2d_sample_unit_word < res->src_basic_unit || dma2d_sample_unit_byte > res->dst_basic_unit || dma2d_sample_unit_word < res->dst_basic_unit
        || inc_mode_increase > res->src_inc_mode || inc_mode_fix < res->src_inc_mode || inc_mode_increase > res->dst_inc_mode || inc_mode_fix < res->dst_inc_mode
        || csk_func_disable > res->src_gather.enable || csk_func_enable < res->src_gather.enable || csk_func_disable > res->dst_scatter.enable || csk_func_enable < res->dst_scatter.enable
        || csk_trigger_null > res->trigger.mode || csk_trigger_dst < res->trigger.mode || csk_func_disable > res->trigger.triggered_en || csk_func_enable < res->trigger.triggered_en
        || dma_2d_ch0 > res->trigger.triggered_src_chn || dma_2d_ch5 < res->trigger.triggered_src_chn) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((dma2d_ch_info[res->dma_ch].status == dma2d_status_none) || (dma2d_ch_info[res->dma_ch].status == dma2d_status_busy)) {
        return CSK_DMA2D_STATUS_ERROR;
    }

    dma2d_ch_info[res->dma_ch].cb_event = cb_event;
    dma2d_ch_info[res->dma_ch].workspace = workspace;
    dma2d_ch_info[res->dma_ch].src_inc_mode = res->src_inc_mode;
    dma2d_ch_info[res->dma_ch].dst_inc_mode = res->dst_inc_mode;

    // Get channel control base address
    uint8_t chn = (uint8_t)res->dma_ch;
    uint32_t* ch_ctrl = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;
    uint32_t ch_ctrl_para = 0;

    // Transfer mode
    ch_ctrl_para |= ((res->tfr_mode&0x3) << DMA2D_CH_CTRL_TFR_MODE_OFFSET);
    if (tfr_mode_p2m == res->tfr_mode || tfr_mode_m2p == res->tfr_mode) {
        // Handshake select
        if (apc_rx3_hs_num15 < res->handshake || rgb_hs_num0 > res->handshake)
            return CSK_DRIVER_ERROR_PARAMETER;
        ch_ctrl_para |= ((res->handshake&0xF) << DMA2D_CH_CTRL_HS_SEL_OFFSET);
        
        // Flow control
        if (dma2d_flow_ctrl_peripheral == res->flow_ctrl) {
            ch_ctrl_para |= ((res->flow_ctrl&0x1) << DMA2D_CH_CTRL_FLOW_CTRL_OFFSET);
        }
    }

    // Basic unit
    ch_ctrl_para |= (((res->src_basic_unit&0x3) << DMA2D_CH_CTRL_SRC_BASE_UNIT_OFFSET) | ((res->dst_basic_unit&0x3) << DMA2D_CH_CTRL_DST_BASE_UNIT_OFFSET));

    // Address mode configure
    ch_ctrl_para |= (((res->src_inc_mode&0x3) << DMA2D_CH_CTRL_SRC_INC_MODE_OFFSET) | ((res->dst_inc_mode&0x3) << DMA2D_CH_CTRL_DST_INC_MODE_OFFSET));

    // Burst length config
    ch_ctrl_para |= (((res->src_burst_len&0x3) << DMA2D_CH_CTRL_SRC_BURST_OFFSET) | ((res->dst_burst_len&0x3) << DMA2D_CH_CTRL_DST_BURST_OFFSET));

    // AHB max burst len
    if (dma2d_ahb_burst_len_64byte < res->rd_max_len && dma2d_ahb_burst_len_256byte >= res->rd_max_len) {
        if (dma_2d_ch0 == res->dma_ch) {
            IP_GPDMA2D->REG_DMA_CH0_RD_MAX_LEN.bit.CFG_RD_MEM_MAX_LEN_00 = res->rd_max_len;
            ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_AHB_LOCK_OFFSET);
        } else if (dma_2d_ch5 == res->dma_ch) {
            IP_GPDMA2D->REG_DMA_CH5_MAX_LEN.all = 0x11;
            IP_GPDMA2D->REG_DMA_CH5_MAX_LEN.bit.CFG_RD_MEM_MAX_LEN_05 = res->rd_max_len;
            ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_AHB_LOCK_OFFSET);
        }
    }
    if (dma2d_ahb_burst_len_64byte < res->wr_max_len && dma2d_ahb_burst_len_256byte >= res->wr_max_len && dma_2d_ch5 == res->dma_ch) {
        IP_GPDMA2D->REG_DMA_CH5_MAX_LEN.all = 0x11;
        IP_GPDMA2D->REG_DMA_CH5_MAX_LEN.bit.CFG_WR_MEM_MAX_LEN_05 = res->wr_max_len;
        ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_AHB_LOCK_OFFSET);
    }

    // Prio configure
    ch_ctrl_para |= ((res->prio_lvl&0x3) << DMA2D_CH_CTRL_CH_PRIO_OFFSET);

    // Source gather
    ch_ctrl_para |= ((res->src_gather.enable&0x1)<<DMA2D_CH_CTRL_CFG_SGEN_OFFSET);
    if (csk_func_enable == res->src_gather.enable) {
        uint32_t *ch_gather_num = (uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_GATHER_NUM_00.all + chn;
        uint32_t *ch_gather_itv = (uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_GATHER_ITV_00.all + chn;
        *ch_gather_num = (res->src_gather.counter&0xFFFFFF);
        *ch_gather_itv = (res->src_gather.interval&0xFFFFFF);
    }

    // Destination scatter
    ch_ctrl_para |= ((res->dst_scatter.enable&0x1)<<DMA2D_CH_CTRL_CFG_DSEN_OFFSET);
    if (csk_func_enable == res->dst_scatter.enable) {
        uint32_t *ch_scatter_num = (uint32_t*)&IP_GPDMA2D->REG_DMA_DST_SCATTER_NUM_00.all + chn;
        uint32_t *ch_scatter_itv = (uint32_t*)&IP_GPDMA2D->REG_DMA_DST_SCATTER_ITV_00.all + chn;
        *ch_scatter_num = (res->dst_scatter.counter&0xFFFFFF);
        *ch_scatter_itv = (res->dst_scatter.interval&0xFFFFFF);
    }

    // Triggered
    uint32_t *ch_triggered = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_TRIGGERED_00.all + chn;
    *ch_triggered = (((res->trigger.triggered_en&0x1) << DMA2D_TRIGGERED_EN_OFFSET) | ((res->trigger.triggered_src_chn&0x7) << DMA2D_TRIGGERED_SRC_OFFSET));
    if (csk_trigger_src == res->trigger.mode)
        ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_ONLINE_D_SRC_OFFSET);
    else if (csk_trigger_dst == res->trigger.mode)
        ch_ctrl_para |= (0x1 << DMA2D_CH_CTRL_ONLINE_D_DST_OFFSET);

    *ch_ctrl = ch_ctrl_para;

    dma2d_ch_info[res->dma_ch].status = dma2d_status_config;

    return CSK_DRIVER_OK;
}

/**
 * @brief Start normal DMA2D transfer
 *
 * @param chn Channel number to start
 * @param src Source memory address
 * @param dst Destination memory address
 * @param img_in_len Input image length
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Start_Normal(csk_dma2d_ch_t chn, void* src, void* dst, uint32_t img_in_len) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Only in configure status can start normal transmits
//    if ((dma2d_ch_info[chn].status != dma2d_status_config) && (dma2d_ch_info[chn].status != dma2d_status_config_img)) {
//        //return CSK_DMA2D_STATUS_ERROR;
//    }

    // Shadow register can be written only when channel's SHD_RDY is 0
    if (*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_SHD_RDY_00.all + chn)) {
        return CSK_DRIVER_ERROR_BUSY;
    }

    dma2d_ch_info[chn].length = img_in_len;
    dma2d_ch_info[chn].xfer_length = 0;
    dma2d_ch_info[chn].status = dma2d_status_busy;

    // Configure source and destination
    *((uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_ADDR_00.all + chn) = (uint32_t)src;
    *((uint32_t*)&IP_GPDMA2D->REG_DMA_DST_ADDR_00.all + chn) = (uint32_t)dst;

    // Configure length
    if (img_in_len > DMA2D_CH_MAX_BLOCK_LENGTH)
        *((uint32_t*)&IP_GPDMA2D->REG_DMA_BLK_LEN_00.all + chn) = DMA2D_CH_MAX_BLOCK_LENGTH;
    else
        *((uint32_t*)&IP_GPDMA2D->REG_DMA_BLK_LEN_00.all + chn) = img_in_len;

    // Enable interrupt
    *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_EN_00.all + chn) = 0x1;

    // Enable
    if (!(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn) & 0x1)) {
        *((uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn) |= 0x1;
    }

    // Enable shadow ready register and start update
    *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_SHD_RDY_00.all + chn) = 0x1;

    return CSK_DRIVER_OK;
}

/**
 * @brief DMA2D interrupt handler for specified channel
 *
 * @param chn Channel number that generated interrupt
 */
static void DMA2D_IRQ_Handler(csk_dma2d_ch_t chn) {
    volatile uint32_t int_status = *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_STATUS_00.all + chn);
    uint32_t* int_clr = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_CLR_00.all + chn;

    // Clear interrupt pending
    *int_clr = int_status;

    uint32_t event = 0;
    csk_dma2d_ch_info_t* p_channel = &dma2d_ch_info[chn];
    uint32_t* ch_status = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_STATUS_00.all + chn;
    bool ch_idle = (*ch_status & 0x1);

    if (int_status&0x1) //blk done irq
    {
        // block transfer done
        event |= CSK_DMA2D_EVENT_BLOCK_DONE;
        p_channel->total_xfer_length += p_channel->last_blk.blk_len;
        //CLOGD("BLK_T_xfer: %d\n", p_channel->total_xfer_length);

        // Need transfer residue data
        if ((p_channel->length - p_channel->xfer_length) > DMA2D_CH_MAX_BLOCK_LENGTH) {
            uint32_t* channel_control = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;
            uint32_t* channel_length = (uint32_t*)&IP_GPDMA2D->REG_DMA_BLK_LEN_00.all + chn;
            uint32_t* ch_shd = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_SHD_RDY_00.all + chn;

            p_channel->xfer_length += *channel_length;

            // Calculate residue size
            if ((p_channel->length - p_channel->xfer_length) >= DMA2D_CH_MAX_BLOCK_LENGTH)
                *channel_length = DMA2D_CH_MAX_BLOCK_LENGTH;
            else
                *channel_length = (p_channel->length - p_channel->xfer_length);

            uint32_t* src_address0 = (uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_ADDR_00.all + chn;
            uint32_t* dst_address0 = (uint32_t*)&IP_GPDMA2D->REG_DMA_DST_ADDR_00.all + chn;
            if (inc_mode_increase == p_channel->src_inc_mode)
                *src_address0 += DMA2D_CH_MAX_BLOCK_LENGTH;
            else if (inc_mode_decrease == p_channel->src_inc_mode)
                *src_address0 -= DMA2D_CH_MAX_BLOCK_LENGTH;

            if (inc_mode_increase == p_channel->dst_inc_mode)
                *dst_address0 += DMA2D_CH_MAX_BLOCK_LENGTH;
            else if (inc_mode_decrease == p_channel->dst_inc_mode)
                *dst_address0 -= DMA2D_CH_MAX_BLOCK_LENGTH;

            // Enable shadow ready register
            *ch_shd = 0x1;
            
            // Enable
            if (!(*channel_control & 0x1)) {
                *channel_control |= 0x1;
            }
        } else // It's the last block
        {
            // Transmit complete
            p_channel->xfer_length = p_channel->length;

            // total transfer done for PingPong (multi-block) or non-PingPong if channel is idle
            if (ch_idle)
                event |= CSK_DMA2D_EVENT_TRANSFER_DONE;

            // Change status to <config>
            p_channel->status = dma2d_status_config;

            if (p_channel->cb_event != NULL) {
                p_channel->cb_event(event, p_channel->workspace);
                // clear total_xfer_length after TRANSFER_DONE is handled
                if (event & CSK_DMA2D_EVENT_TRANSFER_DONE)
                    p_channel->total_xfer_length = 0;
            }
        }
    }

    // BLOCK_DONE or TRANSFER_DONE should be reported first and separated from other interrupt events!
    if (int_status&0x4) //shadow register load irq
    {
        //event = CSK_DMA2D_EVENT_SHD_LOAD_DONE;

        // retrieve the latest block length when WORK registers have been loaded
        csk_dma2d_work_param_t * blk_p = &p_channel->last_blk;
        blk_p->blk_len = (*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn)) & GPDMA2D_DMA_CH_WK_REG6_RPT_00_CFG_WK_BLK_LEN_00_Msk;
        blk_p->src_addr = (void*)(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn));
        blk_p->dst_addr = (void*)(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn));
        //CLOGD("Load: %d\n", blk_p->blk_len);

        // Callback
        //if (p_channel->cb_event != NULL)
        //    p_channel->cb_event(event, p_channel->workspace);
    }

}

/**
 * @brief Channel 0 interrupt handler
 */
static void DMA2D_CHN0_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch0);
}

/**
 * @brief Channel 1 interrupt handler
 */
static void DMA2D_CHN1_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch1);
}

/**
 * @brief Channel 2 interrupt handler
 */
static void DMA2D_CHN2_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch2);
}

/**
 * @brief Channel 3 interrupt handler
 */
static void DMA2D_CHN3_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch3);
}

/**
 * @brief Channel 4 interrupt handler
 */
static void DMA2D_CHN4_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch4);
}

/**
 * @brief Channel 5 interrupt handler
 */
static void DMA2D_CHN5_IRQ_Handler(void) {
    return DMA2D_IRQ_Handler(dma_2d_ch5);
}

/**
 * @brief Common DMA2D interrupt handler
 */
static void DMA2D_COM_IRQ_Handler(void) {
    volatile uint32_t int_status = IP_GPDMA2D->REG_DMA_COM_IRQ_STATUS.all;

    // Clear interrupt pending
    IP_GPDMA2D->REG_DMA_COM_IRQ_CLR.all = int_status;

    return;
}

/**
 * @brief Initialize DMA2D controller
 *
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Initialize(void) {
    uint8_t i = 0;
    uint32_t *ch_irq_clr = NULL;
    uint32_t *ch_irq_en = NULL;
    uint32_t *ch_irq_mask = NULL;
    static volatile uint8_t init_flg = 0;
    static void (*chn_irq_handler[CSK_DMA2D_MAX_CHANNEL_NUM])(void) = 
    {
        DMA2D_CHN0_IRQ_Handler,
        DMA2D_CHN1_IRQ_Handler,
        DMA2D_CHN2_IRQ_Handler,
        DMA2D_CHN3_IRQ_Handler,
        DMA2D_CHN4_IRQ_Handler,
        DMA2D_CHN5_IRQ_Handler
    };

    if (init_flg == 0) {
        memset(dma2d_ch_info, 0, sizeof(dma2d_ch_info));

        // Enable clock
        __HAL_CRM_GPDMA_CLK_ENABLE();

        // Reset
        IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPDMA2D_RESET = 1;

        // Clear chn interrupt and config & disable chn interrupt
        ch_irq_clr = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_CLR_00.all;
        ch_irq_en = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_EN_00.all;
        ch_irq_mask = (uint32_t*)&IP_GPDMA2D->REG_DMA_CH_IRQ_MASK_00.all;
        for (i = 0; i < CSK_DMA2D_MAX_CHANNEL_NUM; i++) {
            *(ch_irq_clr + i) = 0xF;
            *(ch_irq_en + i) = 0x0;
            *(ch_irq_mask + i) = 0x1A;
        }
        IP_GPDMA2D->REG_DMA_CH_CLR.bit.CFG_CH_CLR = 0x3F;
        
        // Register chn global interrupt
        for (i = 0; i < CSK_DMA2D_MAX_CHANNEL_NUM; i++) {
            register_ISR(IRQ_GPDMA_0_VECTOR+i, chn_irq_handler[i], NULL);
            enable_IRQ(IRQ_GPDMA_0_VECTOR+i);
            dma2d_ch_info[i].status = dma2d_status_init;
        }

        // Clear com interrupt and cmd fifo
        IP_GPDMA2D->REG_DMA_COM_IRQ_CLR.all = 0xF;
        IP_GPDMA2D->REG_DMA_CMD_FIFO0_CTRL.bit.CFG_CMD_FIFO0_CLR = 0x1;
        IP_GPDMA2D->REG_DMA_CMD_FIFO1_CTRL.bit.CFG_CMD_FIFO1_CLR = 0x1;
        
        // Disable com interrupt
        IP_GPDMA2D->REG_DMA_COM_IRQ_EN.bit.CFG_COM_IRQ_EN = 0x0;

        // Register com global interrupt
        register_ISR(IRQ_GPDMA_COM_VECTOR, DMA2D_COM_IRQ_Handler, NULL);
        enable_IRQ(IRQ_GPDMA_COM_VECTOR);

        init_flg = 1;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Calculate scaler parameters for image resizing
 *
 * @param in_value Input dimension (width/height)
 * @param out_value Output dimension (width/height)
 * @param scaler_mode Pointer to store scaling mode
 * @param merge_mode Pointer to store merging mode
 * @param phase_step Pointer to store phase step value
 */
void DMA2D_Image_Scaler_Register_Convert(uint16_t in_value, uint16_t out_value, csk_scaler_mode_t *scaler_mode, csk_merge_ratio_t *merge_mode, uint16_t *phase_step) {
    float ratio = 0;

    *scaler_mode = csk_scaler_null;
    *merge_mode = csk_merge_null;

    if (in_value < out_value) {
        *scaler_mode = csk_scaler_up;
    } else if (in_value > out_value) {
        ratio = (float)in_value/out_value;
        if (16 <= ratio) {
            *merge_mode = csk_merge_16to1;
            in_value /= 16;
        } else if (8 <= ratio) {
            *merge_mode = csk_merge_8to1;
            in_value /= 8;
        } else if (4 <= ratio) {
            *merge_mode = csk_merge_4to1;
            in_value /= 4;
        } else if (2 <= ratio) {
            *merge_mode = csk_merge_2to1;
            in_value /= 2;
        }

        if (0 != in_value%out_value) {
            *scaler_mode = csk_scaler_down;
        }
    }

    *phase_step = (float)in_value/out_value*4096;

    //CLOGE("[%s][%d]in=%u,out=%u,scaler=%d,merge=%d,step=%u", __FUNCTION__, __LINE__, in_value, out_value, *scaler_mode, *merge_mode, *phase_step);

    return;
}

/**
 * @brief Calculate scaler parameters for image resizing about max width limit
 *
 * @param in_value Input dimension (width/height)
 * @param out_value Output dimension (width/height)
 * @param scaler_mode Pointer to store scaling mode
 * @param merge_mode Pointer to store merging mode
 * @param phase_step Pointer to store phase step value
 */
void DMA2D_Image_Scaler_Register_Convert_Ext(uint16_t in_value, uint16_t out_value, csk_scaler_mode_t *scaler_mode, csk_merge_ratio_t *merge_mode, uint16_t *phase_step) {
    float ratio = 0;

    *scaler_mode = csk_scaler_null;
    *merge_mode = csk_merge_null;

    // merge check
    if (DMA2D_SCALER_MAX_WIDTH_HALF < in_value || in_value > out_value) {
        ratio = (float)in_value/DMA2D_SCALER_MAX_WIDTH_HALF;
        if (16 <= ratio) {
            *merge_mode = csk_merge_16to1;
            in_value /= 16;
        } else if (8 <= ratio) {
            *merge_mode = csk_merge_8to1;
            in_value /= 8;
        } else if (4 <= ratio) {
            *merge_mode = csk_merge_4to1;
            in_value /= 4;
        } else if (2 <= ratio) {
            *merge_mode = csk_merge_2to1;
            in_value /= 2;
        }
    }

    // scaler check
    if (in_value < out_value) {
        *scaler_mode = csk_scaler_up;
    } else if (in_value > out_value) {
        *scaler_mode = csk_scaler_down;
    }

    *phase_step = (float)in_value/out_value*4096;

//    CLOGD("[%s][%d]in=%u,out=%u,scaler=%d,merge=%d,step=%u", __FUNCTION__, __LINE__, in_value, out_value, *scaler_mode, *merge_mode, *phase_step);

    return;
}

/**
 * @brief Convert image format between internal representation and hardware registers
 *
 * @param format_in Input image format
 * @param format_out Pointer to output format register value
 * @param yuv_type Pointer to store YUV subformat type
 * @param rgb_type Pointer to store RGB endianness type
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Image_PixelFormat_Register_Convert(csk_image_format_t format_in, uint8_t *format_out, uint8_t *yuv_type, uint8_t *rgb_type) {
    *yuv_type = 0x0;
    *rgb_type = 0x0;

    switch (format_in) {
        case csk_image_format_rgb888: {
            *format_out = 0x0;
            *rgb_type = 0x1;
        } break;

        case csk_image_format_bgr888: {
            *format_out = 0x0;
            *rgb_type = 0x0;
        } break;

        case csk_image_format_rgb565: {
            *format_out = 0x1;
            *rgb_type = 0x1;
        } break;

        case csk_image_format_bgr565: {
            *format_out = 0x1;
            *rgb_type = 0x0;
        } break;

        case csk_image_format_yuv444_packed: {
            *format_out = 0x2;
        } break;

        case csk_image_format_yuv422_yuyv_packed: {
            *format_out = 0x3;
            *yuv_type = 0x0;
        } break;

        case csk_image_format_yuv422_uyvy_packed: {
            *format_out = 0x3;
            *yuv_type = 0x1;
        } break;

        case csk_image_format_yuv422_yvyu_packed: {
            *format_out = 0x3;
            *yuv_type = 0x2;
        } break;

        case csk_image_format_yuv422_vyuy_packed: {
            *format_out = 0x3;
            *yuv_type = 0x3;
        } break;

        case csk_image_format_y8: {
            *format_out = 0x4;
        } break;

        default: {
            return CSK_DRIVER_ERROR_PARAMETER;
        } break;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Extended image configuration for DMA2D channels
 *
 * @param chn Channel number to configure
 * @param img_cfg Pointer to image configuration structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 */
int32_t DMA2D_Image_Config_Extend(csk_dma2d_ch_t chn, csk_dma_2d_image_cfg_t* img_cfg) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn || NULL == img_cfg || csk_jpeg_bypass > img_cfg ->img_jpeg_codec_mode || csk_jpeg_encode < img_cfg ->img_jpeg_codec_mode
        || csk_func_disable > img_cfg ->img_rota_en || csk_func_enable < img_cfg ->img_rota_en || csk_func_disable > img_cfg ->img_mirror_en || csk_func_enable < img_cfg ->img_mirror_en
        || csk_func_disable > img_cfg ->img_copy_en || csk_func_enable < img_cfg ->img_copy_en || csk_func_disable > img_cfg ->img_crop_en || csk_func_enable < img_cfg ->img_crop_en
        || csk_func_disable > img_cfg ->img_scaler_en || csk_func_enable < img_cfg ->img_scaler_en) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint32_t *ch_ctrl = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;

    // Validate channel status
    if (dma2d_ch_info[chn].status != dma2d_status_config) {
        return CSK_DRIVER_ERROR;
    }

    if (dma_2d_ch0 == chn) // ch0 support scaler
    {
        //***************************************************scaler
        IP_GPDMA2D->REG_DMA_SCALER_CTRL0.all = 0x0;
        IP_GPDMA2D->REG_DMA_SCALER_CTRL1.all = 0x0;
        IP_GPDMA2D->REG_DMA_SCALER_CTRL2.all = 0x0;
        //if ((csk_func_enable == img_cfg->img_scaler_en) && (img_cfg->img_input.img_width != img_cfg->img_output.img_width || img_cfg->img_input.img_height != img_cfg->img_output.img_height))
        if (csk_func_enable == img_cfg->img_scaler_en) {
            if (csk_func_enable == img_cfg->img_fragment_en) {
                IP_GPDMA2D->REG_DMA_SCALER_CTRL0.all = ((img_cfg->img_scaler_param.col_overlap_flg&0x1)<<DMA2D_SCALER_COL_OVERLAP_OFFSET)|((img_cfg->img_scaler_param.last_band&0x1)<<DMA2D_SCALER_LAST_BAND_OFFSET)|((img_cfg->img_scaler_param.last_tile&0x1)<<DMA2D_SCALER_LAST_TILE_OFFSET)
                                                        |((img_cfg->img_scaler_param.merge_y&0xF)<<DMA2D_SCALER_MERGE_VIRT_MODE_OFFSET)|((img_cfg->img_scaler_param.merge_x&0xF)<<DMA2D_SCALER_MERGE_HOR_MODE_OFFSET)|((img_cfg->img_scaler_param.merge_flg&0x1)<<DMA2D_SCALER_MERGE_FLAG_OFFSET)
                                                        |((img_cfg->img_scaler_param.scaler_flg&0x1)<<DMA2D_SCALER_FLAG_OFFSET)|((img_cfg->img_scaler_param.scaler_y&0x1)<<DMA2D_SCALER_VERT_DIRT_OFFSET)|((img_cfg->img_scaler_param.scaler_x&0x1)<<DMA2D_SCALER_HOR_DIRT_OFFSET)
                                                        |((img_cfg->img_scaler_en&0x1)<<DMA2D_SCALER_EN_OFFSET);
                IP_GPDMA2D->REG_DMA_SCALER_CTRL1.all = ((img_cfg->img_scaler_param.step_phase_y&0xFFFF)<<DMA2D_SCALER_PHASESTEP_VIRT_OFFSET)|((img_cfg->img_scaler_param.step_phase_x&0xFFFF)<<DMA2D_SCALER_PHASESTEP_HOR_OFFSET);
                IP_GPDMA2D->REG_DMA_SCALER_CTRL2.all = ((img_cfg->img_scaler_param.start_phase_y&0xFFF)<<DMA2D_SCALER_STARTPHASE_VIRT_OFFSET)|((img_cfg->img_scaler_param.start_phase_x&0xFFF)<<DMA2D_SCALER_STARTPHASE_HOR_OFFSET);
            } else {
                uint8_t last_band = 1;
                uint8_t last_tile = 1;
                uint8_t merge_flg = 0;
                csk_merge_ratio_t merge_h = csk_merge_null;
                csk_merge_ratio_t merge_v = csk_merge_null;
                uint8_t scaler_flg = 0;
                csk_scaler_mode_t scaler_h = csk_scaler_null;
                csk_scaler_mode_t scaler_v = csk_scaler_null;
                uint16_t phase_step_h = 0;
                uint16_t phase_step_v = 0;

                DMA2D_Image_Scaler_Register_Convert_Ext(img_cfg->img_input.img_width, img_cfg->img_output.img_width, &scaler_h, &merge_h, &phase_step_h);
                DMA2D_Image_Scaler_Register_Convert(img_cfg->img_input.img_height, img_cfg->img_output.img_height, &scaler_v, &merge_v, &phase_step_v);
                if (csk_scaler_null != scaler_h || csk_scaler_null != scaler_v) {
                    scaler_flg = 1;
                }
                if (csk_merge_null != merge_h || csk_merge_null != merge_v) {
                    merge_flg = 1;
                }

                IP_GPDMA2D->REG_DMA_SCALER_CTRL0.all = ((last_band&0x1)<<DMA2D_SCALER_LAST_BAND_OFFSET)|((last_tile&0x1)<<DMA2D_SCALER_LAST_TILE_OFFSET)|((merge_v&0xF)<<DMA2D_SCALER_MERGE_VIRT_MODE_OFFSET)
                                                        |((merge_h&0xF)<<DMA2D_SCALER_MERGE_HOR_MODE_OFFSET)|((merge_flg&0x1)<<DMA2D_SCALER_MERGE_FLAG_OFFSET)|((scaler_flg&0x1)<<DMA2D_SCALER_FLAG_OFFSET)
                                                        |((scaler_v&0x1)<<DMA2D_SCALER_VERT_DIRT_OFFSET)|((scaler_h&0x1)<<DMA2D_SCALER_HOR_DIRT_OFFSET)|((img_cfg->img_scaler_en&0x1)<<DMA2D_SCALER_EN_OFFSET);

                IP_GPDMA2D->REG_DMA_SCALER_CTRL1.all = ((phase_step_v&0xFFFF)<<DMA2D_SCALER_PHASESTEP_VIRT_OFFSET)|((phase_step_h&0xFFFF)<<DMA2D_SCALER_PHASESTEP_HOR_OFFSET);

                //CLOGE("[%s][%d]ctrl0=%#x,ctrl1=%#x,ctrl2=%#x", __FUNCTION__, __LINE__, IP_GPDMA2D->REG_DMA_SCALER_CTRL0.all, IP_GPDMA2D->REG_DMA_SCALER_CTRL1.all, IP_GPDMA2D->REG_DMA_SCALER_CTRL2.all);

                //reserved for scene of frame tile(big resolution exceeds scaler-mode 256*3 linebuffer)
                //uint16_t start_phase_h = 0;
                //uint16_t start_phase_v = 0;
                //IP_GPDMA2D->REG_DMA_SCALER_CTRL2.all = ((start_phase_v&0xFFF)<<DMA2D_SCALER_STARTPHASE_VIRT_OFFSET)|((start_phase_h&0xFFF)<<DMA2D_SCALER_STARTPHASE_HOR_OFFSET);
            }
        }
    } else if (dma_2d_ch1 == chn) // ch1 support rotation/mirror
    {
        //***************************************************rotation
        IP_GPDMA2D->REG_DMA_ROTA_CTRL_01.all &= ~0x1F;
        IP_GPDMA2D->REG_DMA_ROTA_CTRL_01.all |= (((img_cfg->img_rota_en&0x1)<<DMA2D_ROTA_EN_OFFSET)|((img_cfg->img_rota_mode&0x3)<<DMA2D_ROTA_MODE_OFFSET)|((img_cfg->img_rota_tile_en&0x1)<<DMA2D_ROTA_TILE_MODE_OFFSET));

        //***************************************************mirror
        IP_GPDMA2D->REG_DMA_MIRROR_CTRL_01.all &= ~0x3;
        IP_GPDMA2D->REG_DMA_MIRROR_CTRL_01.all |= (((img_cfg->img_mirror_en&0x1)<<DMA2D_MIRROR_EN_OFFSET)|((img_cfg->img_mirror_mode&0x1)<<DMA2D_MIRROR_MODE_OFFSET));
    } else if (dma_2d_ch2 == chn) // ch2 support jpeg encode/decode
    {
        //***************************************************jepg encode and decode
        uint32_t jpeg_image_type = 0;
        IP_GPDMA2D->REG_DMA_JPEG_CTRL_02.all &= ~0xF;
        if (csk_image_format_yuv422_yuyv_packed == img_cfg->img_input.img_format || csk_image_format_yuv422_uyvy_packed == img_cfg->img_input.img_format || csk_image_format_yuv422_yvyu_packed == img_cfg->img_input.img_format || csk_image_format_yuv422_vyuy_packed == img_cfg->img_input.img_format)
            jpeg_image_type = 0x0;
        else if (csk_image_format_yuv444_packed == img_cfg->img_input.img_format || csk_image_format_rgb888 == img_cfg->img_input.img_format || csk_image_format_bgr888 == img_cfg->img_input.img_format)
            jpeg_image_type = 0x1;
        else //y8
            jpeg_image_type = 0x2;
        IP_GPDMA2D->REG_DMA_JPEG_CTRL_02.all |= (((img_cfg->img_jpeg_codec_mode&0x3)<<DMA2D_JPEG_DEC_EN_OFFSET)|((jpeg_image_type&0x3)<<DMA2D_JPEG_DATA_TYPE_OFFSET));
    }

    //***************************************************image format
    uint8_t in_format = 0x0;
    uint8_t in_yuvtype = 0x0;
    uint8_t in_rgbtype = 0x0;
    uint8_t out_format = 0x0;
    uint8_t out_yuvtype = 0x0;
    uint8_t out_rgbtype = 0x0;
    DMA2D_Image_PixelFormat_Register_Convert(img_cfg->img_input.img_format, &in_format, &in_yuvtype, &in_rgbtype);
    DMA2D_Image_PixelFormat_Register_Convert(img_cfg->img_output.img_format, &out_format, &out_yuvtype, &out_rgbtype);
    uint32_t *ch_img_format = (uint32_t*)&IP_GPDMA2D->REG_DMA_IMAGE_FORMA_00.all + chn;
    *ch_img_format &= ~0x3FFF;
    *ch_img_format |= (((in_format&0x7)<<DMA2D_IMAGE_IN_FORMAT_OFFSET)|((out_format&0x7)<<DMA2D_IMAGE_OUT_FORMAT_OFFSET)|((in_yuvtype&0x3)<<DMA2D_IMAGE_YUV422_FORMAT_IN_OFFSET)
                      |((out_yuvtype&0x1)<<DMA2D_IMAGE_YUV422_FORMAT_OUT_OFFSET)|((in_rgbtype&0x1)<<DMA2D_IMAGE_RGB_FORMAT_IN_OFFSET)|((out_rgbtype&0x1)<<DMA2D_IMAGE_RGB_FORMAT_OUT_OFFSET));

    //***************************************************image size
    uint32_t *ch_img_size_in = (uint32_t*)&IP_GPDMA2D->REG_DMA_IN_IMAGE_SIZE_00.all + chn;
    uint32_t *ch_img_size_out = (uint32_t*)&IP_GPDMA2D->REG_DMA_OUT_IMAGE_SIZE_00.all + chn;
    uint32_t *ch_img_size_stride = (uint32_t*)&IP_GPDMA2D->REG_DMA_LINE_STRIDE_SIZE_00.all + chn;
    *ch_img_size_in = 0x0;
    *ch_img_size_in |= (((img_cfg->img_input.img_width&0x7FF)<<DMA2D_SIZE_W_OFFSET)|((img_cfg->img_input.img_height&0x7FF)<<DMA2D_SIZE_H_OFFSET));
    *ch_img_size_out = 0x0;
    *ch_img_size_out |= (((img_cfg->img_output.img_width&0x7FF)<<DMA2D_SIZE_W_OFFSET)|((img_cfg->img_output.img_height&0x7FF)<<DMA2D_SIZE_H_OFFSET));
    *ch_img_size_stride = 0x0;
    *ch_img_size_stride |= (((img_cfg->img_input.img_line_stride&0x3FFF)<<DMA2D_LINE_STR_IN_OFFSET)|((img_cfg->img_output.img_line_stride&0x3FFF)<<DMA2D_LINE_STR_OUT_OFFSET));

    //two-dimensional mode set(scaler/format convertion/crop/jpeg enc/jpeg dec/rotation/mirror/memory copy),one-dimensional mode(normal/scatter/gather)
    //if (csk_func_enable == img_cfg->img_scaler_en || csk_jpeg_decode == img_cfg->img_jpeg_codec_mode || csk_jpeg_encode == img_cfg->img_jpeg_codec_mode || csk_func_enable == img_cfg->img_rota_en
    //    || csk_func_enable == img_cfg->img_mirror_en || csk_func_enable == img_cfg->img_copy_en || csk_func_enable == img_cfg->img_crop_en || img_cfg->img_input.img_format != img_cfg->img_output.img_format)
        *ch_ctrl |= (0x1<<DMA2D_CH_CTRL_LINE_STR_OFFSET);
    //else
    //    *ch_ctrl &= ~(0x1<<DMA2D_CH_CTRL_LINE_STR_OFFSET);

    dma2d_ch_info[chn].status |= dma2d_status_config_img;

    return CSK_DRIVER_OK;
}

int32_t DMA2D_Get_LastBlock(csk_dma2d_ch_t chn, csk_dma2d_work_param_t *param) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn || NULL == param) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

/*
    // get channel work register
    param->src_addr = (void*)(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG0_RPT_00.all + chn));
    param->dst_addr = (void*)(*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG1_RPT_00.all + chn));
    param->blk_len = (*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_WK_REG6_RPT_00.all + chn))&GPDMA2D_DMA_CH_WK_REG6_RPT_00_CFG_WK_BLK_LEN_00_Msk;
*/
    // get channel last work register
    csk_dma2d_work_param_t * blk_p = &dma2d_ch_info[chn].last_blk;
    param->src_addr = blk_p->src_addr;
    param->dst_addr = blk_p->dst_addr;
    param->blk_len = blk_p->blk_len;

    return CSK_DRIVER_OK;
}

int32_t DMA2D_Get_XferredCnt(csk_dma2d_ch_t chn, uint32_t* xfer_len)
{
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn || xfer_len == NULL) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    csk_dma2d_ch_info_t* p_channel = &dma2d_ch_info[chn];
    bool ch_idle = (*((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_STATUS_00.all + chn) & 0x1);

    // get currently transfered data length
    *xfer_len = p_channel->total_xfer_length;
    if (!ch_idle) {
        uint32_t remained_len = *((uint32_t*)&IP_GPDMA2D->REG_DMA_CH_REMAIND_CNT_00.all + chn);
        *xfer_len += p_channel->last_blk.blk_len - remained_len;
    }
    //CLOGD("%s: %d\n", __func__, p_channel->total_xfer_length);

    return CSK_DRIVER_OK;
}

int32_t DMA2D_Config_SrcGather(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn || NULL == param || csk_func_disable > param->enable || csk_func_enable < param->enable) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Get channel control base address
    uint32_t *ch_ctrl = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;
    *ch_ctrl |= ((param->enable&0x1)<<DMA2D_CH_CTRL_CFG_SGEN_OFFSET);
    if (csk_func_enable == param->enable) {
        uint32_t *ch_gather_num = (uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_GATHER_NUM_00.all + chn;
        uint32_t *ch_gather_itv = (uint32_t*)&IP_GPDMA2D->REG_DMA_SRC_GATHER_ITV_00.all + chn;
        *ch_gather_num = (param->counter&0xFFFFFF);
        *ch_gather_itv = (param->interval&0xFFFFFF);
    }

    return CSK_DRIVER_OK;
}

int32_t DMA2D_Config_DstScatter(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param) {
    if (dma_2d_ch0 > chn || dma_2d_ch5 < chn || NULL == param || csk_func_disable > param->enable || csk_func_enable < param->enable) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Get channel control base address
    uint32_t *ch_ctrl = (uint32_t*)&IP_GPDMA2D->REG_DMA_CTRL_00.all + chn;
    *ch_ctrl |= ((param->enable&0x1)<<DMA2D_CH_CTRL_CFG_DSEN_OFFSET);
    if (csk_func_enable == param->enable) {
        uint32_t *ch_scatter_num = (uint32_t*)&IP_GPDMA2D->REG_DMA_DST_SCATTER_NUM_00.all + chn;
        uint32_t *ch_scatter_itv = (uint32_t*)&IP_GPDMA2D->REG_DMA_DST_SCATTER_ITV_00.all + chn;
        *ch_scatter_num = (param->counter&0xFFFFFF);
        *ch_scatter_itv = (param->interval&0xFFFFFF);
    }

    return CSK_DRIVER_OK;
}
