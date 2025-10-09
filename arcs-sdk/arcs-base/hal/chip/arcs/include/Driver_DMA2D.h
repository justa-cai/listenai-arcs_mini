#ifndef CHIP_ARCS_INCLUDE_DRIVER_DMA2D_H_
#define CHIP_ARCS_INCLUDE_DRIVER_DMA2D_H_

#include "Driver_Common.h"
#include "Driver_GPDMA.h"

#define CSK_DMA2D_MAX_CHANNEL_NUM              10
#define CSK_GPDMA_MAX_CHANNEL_NUM              6

typedef enum _csk_dma2d_status {
    dma2d_status_none = 0UL,
    dma2d_status_init = 1UL,
    dma2d_status_config,
	dma2d_status_config_img,
    dma2d_status_busy,
    dma2d_status_stopped,
	dma2d_status_err,
} csk_dma2d_status_t;

typedef enum _csk_dma2d_ch {
    dma_2d_ch6 = 6UL,
    dma_2d_ch7,
    dma_2d_ch8,
    dma_2d_ch9,
} csk_dma2d_ch_t;

typedef enum _csk_dma2d_burst_len {
    dma2d_burst_len_1spl = 0UL,
    dma2d_burst_len_2spl,
    dma2d_burst_len_4spl,
    dma2d_burst_len_8spl,
} csk_dma2d_burst_len_t;

typedef enum _csk_dma2d_sample_unit {
    dma2d_sample_unit_word = 2UL,
} csk_dma2d_sample_unit_t;

#if 0
typedef enum _csk_tfr_mode {
    tfr_mode_p2m = 0UL,
    tfr_mode_m2p,
    tfr_mode_m2m,
} csk_tfr_mode_t;

typedef enum _csk_inc_mode {
    inc_mode_increase = 0UL,
    inc_mode_fix,
} csk_inc_mode_t;

typedef enum _csk_address_mode {
    address_mode_normal = 0UL,
    address_mode_pipo,
} csk_address_mode_t;

typedef enum _csk_prio_mode {
    prio_mode_vhigh = 0UL,
    prio_mode_high,
    prio_mode_normal,
    prio_mode_low,
} csk_prio_mode_t;
#endif

typedef enum _csk_read_done_ack {
	read_done_ack_disable = 0UL,
	read_done_ack_enable = 1UL,
} csk_read_done_ack_t;

#if 0
typedef enum _csk_handshake_num {
    hs_none = 0xff,

    // dma select 0
    qspi_hs_num0 = 0x0,
    d2out_hs_num1,
    d2mask_hs_num2,
    d2back_hs_num3,
    d2force_hs_num4,
    dvp_hs_num5,
    jpg_e_hs_num6,
    jpg_p_hs_num7,
    apc_tx0_hs_num8,
    apc_tx1_hs_num9,
    apc_tx2_hs_num10,
    apc_rx0_hs_num11,
    apc_rx1_hs_num12,
    apc_rx2_hs_num13,
    apc_rx3_hs_num14,
    apc_tx3_hs_num15,

    // dma select 1
    aes_ingress_hs_num6 = 0x16,
    aes_engress_hs_num7 = 0x17,

    // dma select 2
    aes_ingress_hs_num14 = 0x2E,
    aes_engress_hs_num15 = 0x2F,

    hs_max = 0x30,
} csk_handshake_num_t;
#endif

typedef enum _csk_dma2d_flow_ctrl {
    dma2d_flow_ctrl_dma = 0UL,
    dma2d_flow_ctrl_peripheral,
} csk_dma2d_flow_ctrl_t;

#define CSK_DMA2D_EVENT_TRANSFER_DONE                         (1UL << 0)
#define CSK_DMA2D_EVENT_PIPO0_DONE                            (1UL << 1)
#define CSK_DMA2D_EVENT_PIPO1_DONE                            (1UL << 2)

typedef void
(*CSK_DMA2D_SignalEvent_t)(uint32_t event, void* workspace);

#define CSK_DMA2D_STATUS_ERROR         (CSK_DRIVER_ERROR_SPECIFIC - 1)
#define CSK_DMA2D_MODE_ERROR           (CSK_DRIVER_ERROR_SPECIFIC - 2)
#define CSK_DMA2D_UNKNOWN_ERROR        (CSK_DRIVER_ERROR_SPECIFIC - 3)

typedef struct _csk_dma2d_init {
    csk_dma2d_ch_t dma_ch;

    csk_dma2d_burst_len_t burst_len;
    csk_dma2d_sample_unit_t sample_unit;

    csk_address_mode_t src_mode;
    csk_address_mode_t dst_mode;

    csk_tfr_mode_t tfr_mode;
    csk_inc_mode_t src_inc_mode;
    csk_inc_mode_t dst_inc_mode;

    csk_prio_mode_t prio_lvl;
    csk_read_done_ack_t rd_done_ack;

    csk_handshake_num_t handshake;
    csk_dma2d_flow_ctrl_t flow_ctrl;
    csk_dma2d_ch_t trigger_ch;
} csk_dma2d_init_t;

typedef struct _csk_dma2d_info {
    csk_dma2d_status_t status;
    uint32_t cur_src_addr;
    uint32_t cur_dst_addr;
} csk_dma2d_info_t;

/*Image Function*/
typedef enum _csk_image_zoom_scale {
    csk_image_zoom_scale_1_to_1 = 0x0,      // 1:1 scale
	csk_image_zoom_scale_1_to_2,		    // 1:2 scale
	csk_image_zoom_scale_1_to_3,			// 1:3 scale
    csk_image_zoom_scale_1_to_4,            // 1:4 scale
} csk_image_zoom_scale_t;
 
typedef enum _csk_image_input_format {
    csk_image_format_yuv422 = 0x0,
    csk_image_format_yuv420,
    csk_image_format_yuv444,
	csk_image_format_xbgr,
    csk_image_format_xrgb,
	csk_image_format_argb,
	csk_image_format_abgr,
} csk_image_input_format_t;

typedef enum _csk_image_output_format_transfer {
    csk_image_format_transfer_bypass = 0x0,

	csk_image_format_transfer_yuv422_argb,
    csk_image_format_transfer_yuv422_xrgb,

    csk_image_format_transfer_yuv444_argb,
    csk_image_format_transfer_yuv444_xrgb,
    csk_image_format_transfer_yuv444_yuv422,

    csk_image_format_transfer_yuv420_argb,
    csk_image_format_transfer_yuv420_xrgb,
    csk_image_format_transfer_yuv420_yuv422,

	csk_image_format_transfer_yuv422_xbgr,
	csk_image_format_transfer_yuv422_abgr,
	csk_image_format_transfer_yuv444_abgr,
	csk_image_format_transfer_yuv444_xbgr,

	csk_image_format_transfer_yuv422_y8,
	csk_image_format_transfer_xbgr_y8,
	csk_image_format_transfer_xrgb_y8,

	csk_image_format_transfer_yuv422_crop,
	csk_image_format_transfer_xrgb_crop,
	csk_image_format_transfer_xbgr_crop,
	csk_image_format_transfer_argb_crop,
	csk_image_fromat_transfer_abgr_crop,

	csk_image_format_transfer_xrgb_crop_and_scale,
	csk_image_format_transfer_argb_crop_and_scale,

	csk_image_format_transfer_yuv422_cw_90,
	csk_image_format_transfer_yuv422_ccw_90,
	csk_image_fromat_transfer_yuv422_cw90_to_rgb,
	csk_image_fromat_transfer_yuv422_ccw90_to_rgb,
} csk_image_output_format_transfer_t;

typedef enum _csk_image_yuv422_rotate_mode_t {
    csk_image_yuv422_no_rotate = 0x0,
    csk_image_yuv422_clockwise_rotate,
    csk_image_yuv422_counterclockwise_rotate,
} csk_image_yuv422_rotate_mode_t;

typedef enum _csk_image_yuv422_format {
    csk_image_yuv422_format_y0cby1cr = 0x0,
    csk_image_yuv422_format_cby0cry1,
    csk_image_yuv422_format_y0cry1cb,
    csk_image_yuv422_format_cry0cby1,
} csk_image_yuv422_format_t;

typedef enum _csk_image_rgb888_format {
	csk_image_bgr888_format = 0x0,
	csk_image_rgb888_format,
} csk_image_rgb888_format_t;

typedef enum _csk_image_yuv420_format {
    csk_image_yuv420_format_y0cby1 = 0x0,
    csk_image_yuv420_format_cby0y1,
    csk_image_yuv420_format_y0y1cb,
} csk_image_yuv420_format_t;

typedef enum _csk_jpeg_2d_type {
    csk_jpeg_enc_bypass = 0x0,
    csk_jpeg_enc_unpack,
    csk_jpeg_enc_2d_transfer,
    csk_jpeg_dec_pack,
    csk_jpeg_dec_2d_transfer,
} csk_jpeg_2d_type_t;

typedef struct _csk_dma_2d_image_cfg {
    //Base information
    csk_image_input_format_t img_input_format;      // Format of the image (e.g., YUV422, ARGB)
    uint16_t img_height;                // Height of the image in pixels
    uint16_t img_width;                 // Width of the image in pixels

    //Format transfer
    csk_image_yuv422_format_t img_yuv422_format;        // Specific format for YUV422
    csk_image_yuv420_format_t img_yuv420_format;        // Specific format for YUV420
    csk_image_rgb888_format_t img_rgb888_format;
    csk_image_output_format_transfer_t  img_output_fromat_transfer;   // Transfer format for image processing
    csk_image_yuv422_rotate_mode_t image_yuv422_rotate_mode;

    //Zoom setting
    csk_image_zoom_scale_t img_zoom_scale;              // Scale of image zoom (e.g., 1:1, 1:2, 1:3, 1:4)
    uint16_t start_col;                                 // Starting column for zoom/crop
    uint16_t start_row;                                 // Starting row for zoom/crop
    uint16_t end_col;
    uint16_t end_row;
    csk_jpeg_2d_type_t img_jpeg_2d_type;                // JPEG processing type (e.g., bypass, unpack)
} csk_dma_2d_image_cfg_t;

typedef struct _csk_dma_2d_rotate_cfg {
    csk_dma2d_init_t dma2d_init;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;
} csk_dma_2d_rotate_cfg_t;

int32_t
DMA2D_Initialize(void);

int32_t
DMA2D_Config(csk_dma2d_init_t* res, CSK_DMA2D_SignalEvent_t cb_event, void* workspace);

int32_t
DMA2D_Start_Normal(csk_dma2d_ch_t ch, void* src, void* dst, uint32_t img_in_len);

int32_t
DMA2D_Rotate_Config(csk_dma_2d_rotate_cfg_t* res0, csk_dma_2d_rotate_cfg_t* res1, CSK_DMA2D_SignalEvent_t cb_event_res0, CSK_DMA2D_SignalEvent_t cb_event_res1, void* workspace_res0,  void* workspace_res1);

int32_t
DMA2D_Image_Rotate_Config_Extend(csk_dma_2d_rotate_cfg_t *res0, csk_dma_2d_rotate_cfg_t *res1);

int32_t
DMA2D_Start_Rotate(csk_dma_2d_rotate_cfg_t *res0, csk_dma_2d_rotate_cfg_t *res01, void* src0, void *dst1, uint32_t img_in_len);

int32_t
DMA2D_Stop(csk_dma2d_ch_t ch);

int32_t
DMA2D_Start_PiPo(csk_dma2d_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t img_in_len, uint32_t img_out_len);

int32_t
DMA2D_Image_Config_Extend(csk_dma2d_ch_t ch, csk_dma_2d_image_cfg_t* img_cfg);

int32_t
DMA2D_GetCnt(csk_dma2d_ch_t ch, uint32_t *sample_len);

#endif /* ListenAI_INCLUDE_DRIVER_DMA2D_H_ */
