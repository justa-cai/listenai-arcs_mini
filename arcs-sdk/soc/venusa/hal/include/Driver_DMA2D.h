/*!
 * @file Driver_DMA2D.h
 * @brief DMA2D Hardware Abstraction Layer (HAL) Driver Interface
 *        This file contains the interface definitions for the CSK DMA2D peripheral driver.
 *        It provides functionality for 2D graphics acceleration including image transfer,
 *        rotation, scaling, format conversion, and JPEG codec operations.
 */

#ifndef __DRIVER_DMA2D_H__
#define __DRIVER_DMA2D_H__

#include "Driver_Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup DMA2D
  * @brief DMA2D HAL module driver
  * @{
  */

/** @defgroup DMA2D_Exported_Macros DMA2D Exported Macros
  * @{
  */

/// Maximum number of available DMA2D channels
#define CSK_DMA2D_MAX_CHANNEL_NUM              6

/// Event flag indicating successful transfer completion
#define CSK_DMA2D_EVENT_TRANSFER_DONE          (1UL << 0) // total transfer done for PingPong or non-PingPong
#define CSK_DMA2D_EVENT_BLOCK_DONE             (1UL << 1) // single block transfer done of PingPong or non-PingPong
//#define CSK_DMA2D_EVENT_SHD_LOAD_DONE          (1UL << 2) // shadow load done (DMA transfer will be started)

/// Base error code for DMA2D specific errors
#define CSK_DMA2D_STATUS_ERROR                 (CSK_DRIVER_ERROR_SPECIFIC - 1)
#define CSK_DMA2D_MODE_ERROR                   (CSK_DRIVER_ERROR_SPECIFIC - 2)
#define CSK_DMA2D_UNKNOWN_ERROR                (CSK_DRIVER_ERROR_SPECIFIC - 3)
/**
  * @}
  */ /* End of group DMA2D_Exported_Macros */

/* Exported types ------------------------------------------------------------*/
/** @defgroup DMA2D_Exported_Types DMA2D Exported Types
  * @{
  */

/**
 * @enum csk_dma2d_status_t
 * @brief DMA2D module operational status codes
 * @var dma2d_status_none No active operation
 * @var dma2d_status_init Initialization phase
 * @var dma2d_status_config Configuration phase
 * @var dma2d_status_config_img Image configuration phase
 * @var dma2d_status_busy Busy executing operation
 * @var dma2d_status_stopped Operation stopped
 * @var dma2d_status_err Error occurred during operation
 */
typedef enum _csk_dma2d_status {
    dma2d_status_none = 0x0,   ///< No active operation
    dma2d_status_init,        ///< Initializing hardware
    dma2d_status_config,     ///< Configuring registers
    dma2d_status_config_img, ///< Setting up image parameters
    dma2d_status_busy,       ///< Currently processing data
    dma2d_status_stopped,    ///< Operation halted
    dma2d_status_err         ///< Error detected during operation
} csk_dma2d_status_t;

/**
 * @enum csk_dma2d_ch_t
 * @brief Available DMA2D channels with supported features
 * @var dma_2d_ch0 Supports crop/copy/scatter/gather/scaler/format conversion
 * @var dma_2d_ch1 Adds rotation and mirror capabilities
 * @var dma_2d_ch2 Includes JPEG codec support
 * @var dma_2d_ch3 Format conversion focused channel
 * @var dma_2d_ch4 General purpose channel
 * @var dma_2d_ch5 Basic transfer channel
 */
typedef enum _csk_dma2d_ch {
    dma_2d_ch0 = 0x0,             // Full feature set including crop/copy/scatter/gather/scaler/format conv
    dma_2d_ch1,                   // Adds rotation/mirror capabilities
    dma_2d_ch2,                   // Includes JPEG codec functionality
    dma_2d_ch3,                   // Specialized for format conversions
    dma_2d_ch4,                   // Standard transfer channel
    dma_2d_ch5                    // Basic transfer capabilities
} csk_dma2d_ch_t;

/**
 * @enum csk_dma2d_burst_len_t
 * @brief Data burst length configurations for memory accesses
 * @var dma2d_burst_len_1spl Single beat per transfer
 * @var dma2d_burst_len_4spl Four beat burst
 * @var dma2d_burst_len_8spl Eight beat burst
 * @var dma2d_burst_len_16spl Sixteen beat burst
 */
typedef enum _csk_dma2d_burst_len {
    dma2d_burst_len_1spl = 0x0,   ///< Single beat per transfer
    dma2d_burst_len_4spl,        ///< Four beat burst
    dma2d_burst_len_8spl,        ///< Eight beat burst
    dma2d_burst_len_16spl        ///< Sixteen beat burst
} csk_dma2d_burst_len_t;

/**
 * @enum csk_dma2d_ahb_burst_len_t
 * @brief AHB bus burst length configurations
 * @var dma2d_ahb_burst_len_default Use default system settings
 * @var dma2d_ahb_burst_len_64byte 64-byte burst
 * @var dma2d_ahb_burst_len_128byte 128-byte burst
 * @var dma2d_ahb_burst_len_256byte 256-byte burst
 */
typedef enum _csk_dma2d_ahb_burst_len {
    dma2d_ahb_burst_len_default = 0x0, ///< System default burst configuration
    dma2d_ahb_burst_len_64byte,       ///< 64-byte burst size
    dma2d_ahb_burst_len_128byte,      ///< 128-byte burst size
    dma2d_ahb_burst_len_256byte       ///< 256-byte burst size
} csk_dma2d_ahb_burst_len_t;

/**
 * @enum csk_dma2d_sample_unit_t
 * @brief Source/destination data sampling unit sizes
 * @var dma2d_sample_unit_byte Byte (8-bit) samples
 * @var dma2d_sample_unit_halfword Halfword (16-bit) samples
 * @var dma2d_sample_unit_word Word (32-bit) samples
 */
typedef enum _csk_dma2d_sample_unit {
    dma2d_sample_unit_byte = 0x0,   ///< Byte (8-bit) sampling
    dma2d_sample_unit_halfword,    ///< Halfword (16-bit) sampling
    dma2d_sample_unit_word         ///< Word (32-bit) sampling
} csk_dma2d_sample_unit_t;

/**
 * @enum csk_dma2d_flow_ctrl_t
 * @brief Data flow control mechanisms
 * @var dma2d_flow_ctrl_dma DMA controller manages data flow
 * @var dma2d_flow_ctrl_peripheral Peripheral device controls data flow
 */
typedef enum _csk_dma2d_flow_ctrl {
    dma2d_flow_ctrl_dma = 0x0,     ///< DMA controls data movement
    dma2d_flow_ctrl_peripheral     ///< Peripheral masters data flow
} csk_dma2d_flow_ctrl_t;

/**
 * @enum csk_tfr_mode_t
 * @brief Data transfer modes between memories
 * @var tfr_mode_p2m Peripheral to Memory transfer
 * @var tfr_mode_m2p Memory to Peripheral transfer
 * @var tfr_mode_m2m Memory to Memory transfer
 */
typedef enum _csk_tfr_mode {
    tfr_mode_p2m = 0x0,           ///< Peripheral -> Memory transfer
    tfr_mode_m2p,               ///< Memory -> Peripheral transfer
    tfr_mode_m2m                ///< Memory -> Memory transfer
} csk_tfr_mode_t;

/**
 * @enum csk_inc_mode_t
 * @brief Address increment modes for source/destination pointers
 * @var inc_mode_increase Increment address after each transfer
 * @var inc_mode_decrease Decrement address after each transfer
 * @var inc_mode_fix Keep address constant
 */
typedef enum _csk_inc_mode {
    inc_mode_increase = 0x0,     ///< Increment address after transfer
    inc_mode_decrease,          ///< Decrement address after transfer
    inc_mode_fix                ///< Maintain fixed address
} csk_inc_mode_t;

/**
 * @enum csk_prio_mode_t
 * @brief Priority levels for DMA transfer requests
 * @var prio_mode_vhigh Very High priority
 * @var prio_mode_high High priority
 * @var prio_mode_normal Normal priority
 * @var prio_mode_low Low priority
 */
typedef enum _csk_prio_mode {
    prio_mode_vhigh = 0x0,      ///< Highest priority level
    prio_mode_high,            ///< High priority level
    prio_mode_normal,         ///< Standard priority level
    prio_mode_low             ///< Lowest priority level
} csk_prio_mode_t;

/**
 * @enum csk_handshake_num_t
 * @brief Handshake signal selection for synchronization
 * @var rgb_hs_num0 RGB handshake signal 0
 * @var qspi_out_hs_num1 QSPI output handshake 1
 * @var qspi_in_hs_num2 QSPI input handshake 2
 * @var dvp_hs_num3 DVP handshake 3
 * @var jpg_e_hs_num4 JPEG encoder handshake 4
 * @var jpg_p_hs_num5 JPEG parser handshake 5
 * @var uart0_rx_hs_num6 UART0 receive handshake 6
 * @var uart0_tx_hs_num7 UART0 transmit handshake 7
 * @var apc_tx0_hs_num8 APC transmitter 0 handshake 8
 * @var apc_tx1_hs_num9 APC transmitter 1 handshake 9
 * @var apc_tx2_hs_num10 APC transmitter 2 handshake 10
 * @var i8080_hs_num11 I8080 interface handshake 11
 * @var apc_rx0_hs_num12 APC receiver 0 handshake 12
 * @var apc_rx1_hs_num13 APC receiver 1 handshake 13
 * @var apc_rx2_hs_num14 APC receiver 2 handshake 14
 * @var apc_rx3_hs_num15 APC receiver 3 handshake 15
 * @var hs_none No handshake signal (default)
 */
typedef enum _csk_handshake_num {
    rgb_hs_num0 = 0x0,
    qspi_out_hs_num1,
    qspi_in_hs_num2,
    dvp_hs_num3,
    jpg_e_hs_num4,
    jpg_p_hs_num5,
    uart0_rx_hs_num6,
    uart0_tx_hs_num7,
    apc_tx0_hs_num8,
    apc_tx1_hs_num9,
    apc_tx2_hs_num10,
    i8080_hs_num11,
    apc_rx0_hs_num12,
    apc_rx1_hs_num13,
    apc_rx2_hs_num14,
    apc_rx3_hs_num15,
    hs_none = 0xff           ///< No handshake signaling
} csk_handshake_num_t;

/**
 * @enum csk_image_format_t
 * @brief Supported image color formats
 * @var csk_image_format_rgb888 RGB888 format (24-bit)
 * @var csk_image_format_bgr888 BGR888 format (24-bit)
 * @var csk_image_format_rgb565 RGB565 format (16-bit)
 * @var csk_image_format_bgr565 BGR565 format (16-bit)
 * @var csk_image_format_yuv444_packed Packed YUV444 format
 * @var csk_image_format_yuv422_yuyv_packed YUYV packed YUV422 (y0cby1cr)
 * @var csk_image_format_yuv422_uyvy_packed UYVY packed YUV422 (cby0cry1)
 * @var csk_image_format_yuv422_yvyu_packed YVYU packed YUV422 (y0cry1cb)
 * @var csk_image_format_yuv422_vyuy_packed VYUY packed YUV422 (cry0cby1)
 * @var csk_image_format_y8 Grayscale Y8 format
 */
typedef enum _csk_image_format {
    csk_image_format_rgb888 = 0x0,   ///< RGB888 (24-bit)
    csk_image_format_bgr888,        ///< BGR888 (24-bit)
    csk_image_format_rgb565,       ///< RGB565 (16-bit)
    csk_image_format_bgr565,      ///< BGR565 (16-bit)
    csk_image_format_yuv444_packed,///< Packed YUV444
    csk_image_format_yuv422_yuyv_packed,     /* y0cby1cr */
    csk_image_format_yuv422_uyvy_packed,     /* cby0cry1 */
    csk_image_format_yuv422_yvyu_packed,     /* y0cry1cb */
    csk_image_format_yuv422_vyuy_packed,     /* cry0cby1 */
    csk_image_format_y8             ///< Grayscale Y8 format
} csk_image_format_t;

/**
 * @enum csk_jpeg_codec_mode_t
 * @brief JPEG encoding/decoding modes
 * @var csk_jpeg_bypass Passthrough mode (no transformation)
 * @var csk_jpeg_decode Decode JPEG stream to image
 * @var csk_jpeg_encode Encode image to JPEG stream
 */
typedef enum _csk_jpeg_codec_mode {
    csk_jpeg_bypass = 0x0, ///< Passthrough mode
    csk_jpeg_decode,      ///< JPEG decoding
    csk_jpeg_encode       ///< JPEG encoding
} csk_jpeg_codec_mode_t;

/**
 * @enum csk_rotation_mode_t
 * @brief Image rotation modes
 * @var csk_rota_cw90 Rotate 90° clockwise
 * @var csk_rota_ccw90 Rotate 90° counterclockwise
 * @var csk_rota_cw180 Rotate 180° clockwise
 * @var csk_rota_transpose Transpose image (mirror along diagonal)
 */
typedef enum _csk_rotation_mode {
    csk_rota_cw90 = 0x0,        //clockwise 90°
    csk_rota_ccw90,             //counterclockwise 90°
    csk_rota_cw180,             //clockwise 180°
    csk_rota_transpose          //transpose
} csk_rotation_mode_t;

/**
 * @enum csk_mirror_mode_t
 * @brief Image mirroring modes
 * @var csk_mirror_hor Horizontal mirroring
 * @var csk_mirror_vert Vertical mirroring
 */
typedef enum _csk_mirror_mode {
    csk_mirror_hor = 0x0,       //mirror horizontal
    csk_mirror_vert            //mirror vertical
} csk_mirror_mode_t;

/**
 * @enum csk_scaler_mode_t
 * @brief Image scaling modes
 * @var csk_scaler_down Downscale image
 * @var csk_scaler_up Upscale image
 * @var csk_scaler_null No scaling (maintain original size)
 */
typedef enum _csk_scaler_mode {
    csk_scaler_down = 0x0,      //scaler down
    csk_scaler_up,              //scaler up
    csk_scaler_null             //remain unchange
} csk_scaler_mode_t;

/**
 * @enum csk_merge_ratio_t
 * @brief Color merging ratios for downsampling
 * @var csk_merge_null No merging (1:1 mapping)
 * @var csk_merge_2to1 Average 2 pixels into 1
 * @var csk_merge_4to1 Average 4 pixels into 1
 * @var csk_merge_8to1 Average 8 pixels into 1
 * @var csk_merge_16to1 Average 16 pixels into 1
 */
typedef enum _csk_merge_ratio {
    csk_merge_null = 0x0,      //remain unchange
    csk_merge_2to1 = 0x1,      //two datas are averaged
    csk_merge_4to1 = 0x2,      //four datas are averaged
    csk_merge_8to1 = 0x4,      //eight datas are averaged
    csk_merge_16to1 = 0x8      //sixteen datas are averaged
} csk_merge_ratio_t;

/**
 * @enum csk_func_switch_t
 * @brief Function enable/disable switch
 * @var csk_func_disable Disable function
 * @var csk_func_enable Enable function
 */
typedef enum _csk_func_switch {
    csk_func_disable = 0x0,       //function disable
    csk_func_enable              //function enable
} csk_func_switch_t;

/**
 * @enum csk_trigger_mode_t
 * @brief Trigger mechanism configurations
 * @var csk_trigger_null No trigger (manual start)
 * @var csk_trigger_src Trigger on source channel activity
 * @var csk_trigger_dst Trigger on destination channel activity
 */
typedef enum _csk_trigger_mode {
    csk_trigger_null = 0x0,       //trigger disable mode
    csk_trigger_src,              //trigger source channel mode(used to carry image data to sram)
    csk_trigger_dst               //trigger destionation channel mode(used to carry sram image data to peripheral)
} csk_trigger_mode_t;

/**
 * @struct csk_image_info_t
 * @brief Image dimensions and format information
 * @var img_width Image width in pixels
 * @var img_height Image height in pixels
 * @var img_line_stride Line stride in bytes (memory layout parameter)
 * @var img_format Image color format
 */
typedef struct _csk_image_info {
    uint16_t img_width;                             // width of the image (unit:pixel)
    uint16_t img_height;                            // height of the image (unit:pixel)
    uint16_t img_line_stride;                       // stride of the image (unit:byte)
    csk_image_format_t img_format;                  // format of image
} csk_image_info_t;

/**
 * @struct csk_gather_scatter_info_t
 * @brief Gather/Scatter operation configuration
 * @var enable Function enable/disable switch
 * @var interval Data interval between samples (bytes)
 * @var counter Data counter value (bytes)
 */
typedef struct _csk_gather_scatter_info {
    csk_func_switch_t enable;                       // function switch
    uint32_t interval;                              // data interval(unit:byte)
    uint32_t counter;                               // data counter(unit:byte)
} csk_gather_scatter_info_t;

/**
 * @struct csk_trigger_info_t
 * @brief Trigger mechanism configuration
 * @var mode Trigger operation mode
 * @var triggered_en Trigger enable/disable
 * @var triggered_src_chn Source channel for trigger events
 */
typedef struct _csk_trigger_info {
    csk_trigger_mode_t mode;                        // trigger function mode(include switch)
    csk_func_switch_t triggered_en;                 // triggered switch
    csk_dma2d_ch_t triggered_src_chn;               // triggered source dma channel
} csk_trigger_info_t;

/**
 * @struct csk_dma2d_init_t
 * @brief DMA2D initialization and configuration parameters
 * @var dma_ch Channel selection
 * @var tfr_mode Transfer mode (P2M/M2P/M2M)
 * @var src_basic_unit Source data unit size (valid for decreasing mode)
 * @var dst_basic_unit Destination data unit size (valid for decreasing mode)
 * @var src_inc_mode Source address increment mode
 * @var dst_inc_mode Destination address increment mode
 * @var src_burst_len Source burst length (peripheral only)
 * @var dst_burst_len Destination burst length (peripheral only)
 * @var flow_ctrl Data flow control mechanism
 * @var prio_lvl Priority level assignment
 * @var handshake Handshake signal selection
 * @var rd_max_len Read memory AHB max burst length
 * @var wr_max_len Write memory AHB max burst length
 * @var src_gather Source gather configuration
 * @var dst_scatter Destination scatter configuration
 * @var trigger Trigger mechanism configuration
 */
typedef struct _csk_dma2d_init {
    csk_dma2d_ch_t dma_ch;
    csk_tfr_mode_t tfr_mode;
    csk_dma2d_sample_unit_t src_basic_unit;         //when src_inc_mode=inc_mode_decrease,only be used for peripheral devices
    csk_dma2d_sample_unit_t dst_basic_unit;         //when src_inc_mode=inc_mode_decrease,only be used for peripheral devices
    csk_inc_mode_t src_inc_mode;
    csk_inc_mode_t dst_inc_mode;
    csk_dma2d_burst_len_t src_burst_len;            //only be used for peripheral devices
    csk_dma2d_burst_len_t dst_burst_len;            //only be used for peripheral devices
    csk_dma2d_flow_ctrl_t flow_ctrl;
    csk_prio_mode_t prio_lvl;
    csk_handshake_num_t handshake;
    csk_dma2d_ahb_burst_len_t rd_max_len;           //read memory ahb max burst length
    csk_dma2d_ahb_burst_len_t wr_max_len;           //write memory ahb max burst length
    csk_gather_scatter_info_t src_gather;
    csk_gather_scatter_info_t dst_scatter;
    csk_trigger_info_t trigger;
} csk_dma2d_init_t;

/**
 * @struct csk_dma_2d_image_scaler_cfg_t
 * @brief Advanced image scaler configuration
 * @var scaler_flg scaler flg enable/disable
 * @var scaler_x scaler hori direction up/down
 * @var scaler_y scaler vert direction up/down
 * @var merge_flg merge flg enable/disable
 * @var merge_x merge hori mode 2/4/8/16
 * @var merge_y merge vert mode 2/4/8/16
 * @var last_tile one band last tile enable/disable
 * @var last_band one frame last band enable/disable
 * @var col_overlap_flg col overlap flg enable/disable
 * @var step_phase_x move step phase hori in/out*4096
 * @var step_phase_y move step phase vert in/out*4096
 * @var start_phase_x start phase hori
 * @var start_phase_y start phase vert
 */
typedef struct _csk_dma_2d_image_scaler_cfg {
    uint8_t scaler_flg;                             // scaler flg
    csk_scaler_mode_t scaler_x;                     // scaler hori scaler up or down
    csk_scaler_mode_t scaler_y;                     // scaler vert scaler up or down
    uint8_t merge_flg;                              // merge flg
    csk_merge_ratio_t merge_x;                      // hori merge number
    csk_merge_ratio_t merge_y;                      // vert merge number
    uint8_t last_tile;                              // last tile for every band
    uint8_t last_band;                              // last band for every frame
    uint8_t col_overlap_flg;                        // two col data need overlap
    uint16_t step_phase_x;                          // scaler step phase x
    uint16_t step_phase_y;                          // scaler step phase y
    uint16_t start_phase_x;                         // scaler start phase x
    uint16_t start_phase_y;                         // scaler start phase y
} csk_dma_2d_image_scaler_cfg_t;

/**
 * @struct csk_dma_2d_image_cfg_t
 * @brief Advanced image processing configuration
 * @var img_input Input image specifications
 * @var img_output Output image specifications
 * @var img_jpeg_codec_mode JPEG codec operation mode
 * @var img_rota_en Rotation enable/disable
 * @var img_rota_tile_en Tiled rotation mode
 * @var img_rota_mode Rotation angle/direction
 * @var img_mirror_en Mirroring enable/disable
 * @var img_mirror_mode Mirror axis selection
 * @var img_copy_en Direct memory copy enable/disable
 * @var img_crop_en Cropping enable/disable
 * @var img_scaler_en Scaling enable/disable
 */
typedef struct _csk_dma_2d_image_cfg {
    csk_image_info_t img_input;                     // input image info
    csk_image_info_t img_output;                    // output image info

    csk_jpeg_codec_mode_t img_jpeg_codec_mode;      // jpeg codec mode

    csk_func_switch_t img_rota_en;                  // rotation switch
    csk_func_switch_t img_rota_tile_en;             // rotation tile mode switch
    csk_rotation_mode_t img_rota_mode;              // rotation mode

    csk_func_switch_t img_mirror_en;                // mirror switch
    csk_mirror_mode_t img_mirror_mode;              // mirror mode

    csk_func_switch_t img_copy_en;                  // memcpy copy switch

    csk_func_switch_t img_crop_en;                  // memcpy crop switch

    csk_func_switch_t img_scaler_en;                // scaler switch
    csk_func_switch_t img_fragment_en;              // img fragment switch(disable:no-fragment driver automatic control   enable:external fragment control register value)
    csk_dma_2d_image_scaler_cfg_t img_scaler_param; // valid when img_fragment_en=csk_func_enable
} csk_dma_2d_image_cfg_t;

/**
 * @struct csk_dma_2d_rotate_cfg_t
 * @brief Complete rotation operation configuration container
 * @var dma2d_init Basic DMA2D initialization parameters
 * @var dma2d_img_cfg Advanced image processing configuration
 */
typedef struct _csk_dma_2d_rotate_cfg {
    csk_dma2d_init_t dma2d_init;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;
} csk_dma_2d_rotate_cfg_t;

/**
 * @struct csk_dma2d_work_param_t
 * @brief get DMA2D cur work parameters
 * @var src_addr source address
 * @var dst_addr destination address
 * @var blk_len block length,data base unit is byte
 */
typedef struct _csk_dma2d_work_param {
    void *src_addr;
    void *dst_addr;
    uint32_t blk_len;
} csk_dma2d_work_param_t;

/**
 * @typedef CSK_DMA2D_SignalEvent_t
 * @brief Event callback function pointer type
 * @param[in] event Event identification code
 * @param[in] workspace User-defined context data pointer
 */
typedef void (*CSK_DMA2D_SignalEvent_t)(uint32_t event, void* workspace);
/**
  * @}
  */ /* End of group DMA2D_Exported_Types */

/* Exported functions --------------------------------------------------------*/
/** @defgroup DMA2D_Exported_Functions DMA2D Exported Functions
  * @{
  */

/**
 * @fn int32_t DMA2D_Initialize(void)
 * @brief Initializes the DMA2D peripheral with default parameters
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Initialize(void);

/**
 * @fn int32_t DMA2D_Config(csk_dma2d_init_t* res, CSK_DMA2D_SignalEvent_t cb_event, void* workspace)
 * @brief Configures DMA2D channel with specified parameters
 * @param[in] res Pointer to configuration structure
 * @param[in] cb_event Event callback function
 * @param[in] workspace User context data pointer
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Config(csk_dma2d_init_t* res, CSK_DMA2D_SignalEvent_t cb_event, void* workspace);

/**
 * @fn int32_t DMA2D_Start_Normal(csk_dma2d_ch_t chn, void* src, void* dst, uint32_t img_in_len)
 * @brief Starts normal DMA2D transfer operation
 * @param[in] chn Channel number to use
 * @param[in] src Source memory address
 * @param[in] dst Destination memory address
 * @param[in] img_in_len Input image data length
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Start_Normal(csk_dma2d_ch_t chn, void* src, void* dst, uint32_t img_in_len);

/**
 * @fn int32_t DMA2D_Stop(csk_dma2d_ch_t chn)
 * @brief Stops ongoing DMA2D operation on specified channel
 * @param[in] chn Channel number to stop
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Stop(csk_dma2d_ch_t chn);

/**
 * @fn int32_t DMA2D_Image_Config_Extend(csk_dma2d_ch_t chn, csk_dma_2d_image_cfg_t* img_cfg)
 * @brief Applies extended image processing configuration
 * @param[in] chn Channel number to configure
 * @param[in] img_cfg Pointer to advanced image configuration
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Image_Config_Extend(csk_dma2d_ch_t chn, csk_dma_2d_image_cfg_t* img_cfg);

/**
 * @fn int32_t DMA2D_Get_LastBlock(csk_dma2d_ch_t chn, csk_dma2d_work_param_t *param)
 * @brief get latest transferred block (work register param) of DMA2D chn
 * @param[in] chn Channel number to get
 * @param[out] work register param struct
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Get_LastBlock(csk_dma2d_ch_t chn, csk_dma2d_work_param_t *param);

/**
 * @fn DMA2D_Get_XferredCnt(csk_dma2d_ch_t chn, uint32_t* xfer_len);
 * @brief Get current transferred bytes of channel chn since last TRANSFER_DONE event
 * @param[in]  chn       channel number, specifying the DMA2D channel to query
 * @param[out] xfer_len  pointer to queried transferred length (in bytes).
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Get_XferredCnt(csk_dma2d_ch_t chn, uint32_t* xfer_len);

/**
 * @fn int32_t DMA2D_Config_SrcGather(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param)
 * @brief set src gather register param
 * @param[in] chn Channel number to get
 * @param[in] src gather register param struct
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Config_SrcGather(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param);

/**
 * @fn int32_t DMA2D_Config_DstScatter(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param)
 * @brief set src gather register param
 * @param[in] chn Channel number to get
 * @param[in] dst scatter register param struct
 * @return CSK_DRIVER_OK on success, negative error code otherwise
 */
int32_t DMA2D_Config_DstScatter(csk_dma2d_ch_t chn, csk_gather_scatter_info_t *param);
/**
  * @}
  */ /* End of group DMA2D_Exported_Functions */

/** @} */ /* End of DMA2D group */

#ifdef __cplusplus
}
#endif

#endif /* __DRIVER_DMA2D_H__ */
