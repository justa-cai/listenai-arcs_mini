/*******************************************************
 * File Name:    jpeg.c
 * Functionality: Low-level driver implementation for hardware JPEG accelerator,
 *               supporting encoding/decoding operations, interrupt handling,
 *               and device management with singleton pattern.
 * Author:       USER
 * Date Created: 2025.06.06
 * Notes:
 *   1. Singleton pattern enforced - only one JPEG device instance supported.
 *   2. Hardware register mappings must match HW manual specifications.
 *   3. Interrupt priority levels should be configured at system level.
 *   4. Debug logging controlled via DEBUG_LOG macro (defined as 1 by default).
 *   5. All API calls require prior successful Jpeg_Initialize().
 * Dependencies:
 *   - Hardware JPEG acceleration engine
 *   - log_print.h (logging interface)
 *   - jpeg.h (JPEG data structures)
 *******************************************************/

#include <string.h>
#include <stdint.h>
#include "jpeg.h"
#include "log_print.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG

// driver version
#define CSK_JPEG_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION jpeg_driver_version = { CSK_JPEG_API_VERSION, CSK_JPEG_DRV_VERSION };

//------------------------------------------------------------------------------------------

/**
 * @brief Global JPEG device instance (singleton pattern)
 *        Preallocated static device structure mapped to hardware base address
 */
_FAST_DATA_VI static Jpeg_DEV jpeg0_dev = {
        (IP_JPEG_TOP),
        {0},
};

//------------------------------------------------------------------------------------

/**
 * @brief Enable JPEG clock from system controller
 *        Activates peripheral clock gate for JPEG block
 */
static void ap_cfg_jpeg_clk_enable(void)
{
    IP_SYSCTRL->REG_PERI_CLK_CFG7.all |= (1<<CMN_SYSCFG_PERI_CLK_CFG7_ENA_JPEG_CLK_Pos);
}

/**
 * @brief Perform software reset on JPEG block
 *        Drives reset line high to initialize hardware state machine
 */
static void ap_cfg_jpeg_reset(void)
{
    IP_SYSCTRL->REG_SW_RESET_CFG2.all |= (1<<CMN_SYSCFG_SW_RESET_CFG2_JPEG_RESET_Pos);
}

/**
 * @brief Full reset sequence for JPEG subsystem
 *        Coordinates clock enable + reset pulse execution
 */
void Jpeg_Reset(void)
{
    ap_cfg_jpeg_clk_enable();
    ap_cfg_jpeg_reset();
    return;
}

//------------------------------------------------------------------------------------

/**
 * @brief Validate JPEG device pointer integrity
 *        Checks if provided device pointer matches predefined singleton instance
 *        @param[in] jpeg_dev Pointer to device structure to validate
 *        @return Valid Jpeg_DEV* on success, NULL on validation failure
 */
static Jpeg_DEV * safe_jpeg_dev(void *jpeg_dev)
{
    //TODO: safe check of Jpeg device parameter
    if (jpeg_dev == Jpeg0()) {
        if (((Jpeg_DEV *)jpeg_dev)->Instance != (IP_JPEG_TOP)) {
            DEV_LOG("[%s:%d] Error jpeg_dev=%#x ", __func__, __LINE__, jpeg_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error jpeg_dev=%#x ", __func__, __LINE__, jpeg_dev);
        return NULL;
    }

    return (Jpeg_DEV *)jpeg_dev;
}

/**
 * @brief Interrupt handler for JPEG completion events
 *        Processes hardware interrupt status and triggers registered callbacks
 *        @note Called automatically by interrupt vector table
 */
_FAST_FUNC_RO void Jpeg_IRQ_Handler()
{
    Jpeg_DEV *jpeg_dev = (Jpeg_DEV *)Jpeg0();
    uint32_t status = jpeg_dev->Instance->REG_INT_ST_CLR.all;
    //DEV_LOG("[%s:%d] status=0x%x", __func__, __LINE__, status);

    /* irq clear */
    jpeg_dev->Instance->REG_INT_ST_CLR.all = status;

    /* Coding/decoding done interrupt status bit" */
    if((status & JPEG_INT_ST_CLR_CODEC_DONE_Msk) == JPEG_INT_ST_CLR_CODEC_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_CODEC_DONE, 0);
    }

    /* DMA P channel transfer done */
    if((status & JPEG_INT_ST_CLR_PDMA_DONE_Msk) == JPEG_INT_ST_CLR_PDMA_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_PDMA_DONE, 0);
    }

    /* DMA E channel transfer done */
    if((status & JPEG_INT_ST_CLR_EDMA_DONE_Msk) == JPEG_INT_ST_CLR_EDMA_DONE_Msk)
    {
        if(jpeg_dev->cb_event)
            jpeg_dev->cb_event(JPEG_IRQ_EVENT_EDMA_DONE, 0);
    }
}

//---------------------------------------------------------------------------------------
/* Exported functions --------------------------------------------------------*/
/**
 * @brief Get driver version information
 *        Returns combined API and driver versions as defined in header files
 *        @return Version structure containing major/minor version numbers
 */
CSK_DRIVER_VERSION
Jpeg_GetVersion(void)
{
    return jpeg_driver_version;
}

/**
 * @brief Obtain handle to primary JPEG device instance
 *        Provides access to preconfigured singleton device structure
 *        @return Pointer to global jpeg0_dev structure
 */
void* Jpeg0(void)
{
    return &jpeg0_dev;
}

/**
 * @brief Get physical address of pixel buffer memory region
 *        Direct access to hardware-managed frame buffer storage
 *        @return Absolute address of pixel buffer area
 */
uint32_t Jpeg0_PixelBuf(void)
{
    return JPEG_PIXEL_BUF;
}

/**
 * @brief Get physical address of entropy coded segment buffer
 *        Dedicated memory area for compression intermediate data
 *        @return Absolute address of ECS buffer region
 */
uint32_t Jpeg0_ECSBuf(void)
{
    return JPEG_ECS_BUF;
}

/**
 * @brief Get physical address of encoding quantization tables storage
 *        Holds custom quantization matrices used during compression
 *        @return Absolute address of QM table storage
 */
uint32_t Jpeg0_EncodeQuanBuf(void)
{
    return JPEG_QM_BUF;
}

/**
 * @brief Get physical address of encoding Huffman tables storage
 *        Contains precomputed Huffman codesets for compression efficiency
 *        @return Absolute address of Huffman table storage
 */
uint32_t Jpeg0_EncodeHuffBuf(void)
{
    return JPEG_HENC_BUF;
}

/**
 * @brief Get physical address of decoding Min table storage
 *        Lookup tables used during progressive image reconstruction
 *        @return Absolute address of Min table storage
 */
uint32_t Jpeg0_DecodeMinBuf(void)
{
    return JPEG_HM_BUF;
}

/**
 * @brief Get physical address of decoding Base table storage
 *        Fundamental reference values for IDCT calculations
 *        @return Absolute address of Base table storage
 */
uint32_t Jpeg0_DecodeBaseBuf(void)
{
    return JPEG_HB_BUF;
}

/**
 * @brief Get physical address of decoding Symbol table storage
 *        Mapping between symbol codes and actual values
 *        @return Absolute address of Symbol table storage
 */
uint32_t Jpeg0_DecodeSymBuf(void)
{
    return JPEG_HS_BUF;
}

/**
 * @brief Initialize JPEG hardware device
 *        Configures operational mode, register settings, and interrupt handling
 *        @param[in] pJpegDev Pointer to device structure (must be valid)
 *        @param[in] pCallback Event notification callback function
 *        @param[in] pCfg Configuration parameters for initialization
 *        @return CSK_DRIVER_OK on success, error code on failure
 *        @note Must be called before any JPEG operations can be performed
 */
int32_t Jpeg_Initialize(void *pJpegDev, void *pCallback, Jpeg_InitTypeDef *pCfg)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(NULL == pCfg)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Jpeg_Reset();

    memcpy(&jpeg_dev->Init, pCfg, sizeof(Jpeg_InitTypeDef));

    jpeg_dev->Instance->REG_IFCTRL.bit.XMODE= 0x0;             // 0x74 bit1=0 xmode

    switch(pCfg->mode)
    {
        case JPEG_MODE_ENCODE:
            jpeg_dev->Instance->REG_CONTRL.bit.MODE = 0x0;             // 0x04  bit0=0:encoder; 1:decoder
            //jpeg_dev->Instance->REG_JCR1.all = 0x6;                    // 0x803 bit3=0:encoder; 1:decoder
            jpeg_dev->Instance->REG_JCR1.bit.JCR_DE = 0x0;     // 0x804 bit3=0:encoder; 1:decoder
            break;

        case JPEG_MODE_DECODE:
            jpeg_dev->Instance->REG_CONTRL.bit.MODE = 0x1;             // 0x04  bit0=0:encoder; 1:decoder
            //jpeg_dev->Instance->REG_JCR1.all = 0xe;                    // 0x803 bit3=0:encoder; 1:decoder
            jpeg_dev->Instance->REG_JCR1.bit.JCR_DE = 0x1;     // 0x804 bit3=0:encoder; 1:decoder
            break;

        default:
            CLOG("[%s:%d] mode=%d is error", __func__, __LINE__, pCfg->mode);
            return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (pCfg->rst_enable)
        jpeg_dev->Instance->REG_JCR1.bit.JCR_RE = 0x1;           // 0x804 bit2=0:no restart marker insert; 1:hardware insert restart marker
    else
        jpeg_dev->Instance->REG_JCR1.bit.JCR_RE = 0x0;           // 0x804 bit2=0:no restart marker insert; 1:hardware insert restart marker

    switch(pCfg->format_in)
    {
        case JPEG_DECODE_IN_FORMAT_YUV422:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x0;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x1<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV420:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x1;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x3<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV444:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x2;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x0<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_YUV411:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_IN_FORMAT = 0x3;    // 0x04 bit[2:1] = 0:422; 1:420; 2:444; 3:411 dec_in_format
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x2;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x3<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = ((pCfg->ht_index[1]>>4)&0x1)|((pCfg->ht_index[1]&0x1)<<1)|((pCfg->qt_index[1]&0x1)<<2)|(0x0<<4); // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = ((pCfg->ht_index[2]>>4)&0x1)|((pCfg->ht_index[2]&0x1)<<1)|((pCfg->qt_index[2]&0x1)<<2)|(0x0<<4); // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        case JPEG_DECODE_IN_FORMAT_GRAY:
            jpeg_dev->Instance->REG_JCR1.bit.JCR_NOL = 0x0;            // 0x804 bit0~1=color components number minus one
            jpeg_dev->Instance->REG_JCR4.all = ((pCfg->ht_index[0]>>4)&0x1)|((pCfg->ht_index[0]&0x1)<<1)|((pCfg->qt_index[0]&0x1)<<2)|(0x0<<4); // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
            jpeg_dev->Instance->REG_JCR5.all = 0x0;                    // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
            jpeg_dev->Instance->REG_JCR6.all = 0x0;                    // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
            break;

        default:
            CLOG("[%s:%d] format_in=%d is error", __func__, __LINE__, pCfg->format_in);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch(pCfg->format_out)
    {
        case JPEG_DECODE_OUT_FORMAT_YUV422:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_OU_FORMAT = 0x0;    // 0x04 bit[3]   = 0:YUV422; 1:RGB dec_ou_format
            break;

        case JPEG_DECODE_OUT_FORMAT_RGB:
            jpeg_dev->Instance->REG_CONTRL.bit.DEC_OU_FORMAT = 0x1;    // 0x04 bit[3]   = 0:YUV422; 1:RGB dec_ou_format
            break;

        default:
            CLOG("[%s:%d] format_out=%d is error", __func__, __LINE__, pCfg->format_out);
            return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_PIXEL_DMA_TRANSFER_SIZE.bit.PDMA_SIZE = pCfg->pixel_size;    // 0x10 bit[19:0] Pixel data length in a DMA transfer. (not used)
    jpeg_dev->Instance->REG_ECS_DMA_TRANSFER_SIZE.bit.EDMA_SIZE = pCfg->ecs_size;    // 0x14 bit[19:0] ECS data length in a DMA transfer.
    jpeg_dev->Instance->REG_SOURCE_DATA_LENGTH.bit.SRC_LEN = pCfg->src_size;           // 0x18 bit[23:0] The total length of incoming data of a process.
    //jpeg_dev->Instance->REG_RESULT_DATA_LENGTH.bit.RESULT_LEN = 0x800;          // 0x1C bit[23:0] The length of encoder ECS output data reported by JPEG interface.

    jpeg_dev->Instance->REG_ENC_PIC_SIZE.bit.ARCS_ENC_WIDTH = pCfg->img_width;    // 0x68 bit[27:16]: pic width
    jpeg_dev->Instance->REG_ENC_PIC_SIZE.bit.ARCS_ENC_HEIGHT= pCfg->img_height;    // 0x68 bit[10:0]: pic height
    jpeg_dev->Instance->REG_DEC_PIC_SIZE.bit.ARCS_DEC_WIDTH = pCfg->img_width;    // 0x68 bit[27:16]: pic width
    jpeg_dev->Instance->REG_DEC_PIC_SIZE.bit.ARCS_DEC_HEIGHT= pCfg->img_height;    // 0x68 bit[10:0]: pic height

    // 0x804 bit[1:0]: Number of color components in the image data minus one.
    // 0x804 bit2 = 0: no restart marker insert   1: hardware insert restart marker
    // 0x804 bit3 = 0: encoder   1: decoder
    //jpeg_dev->Instance->REG_JCR1.all = 0x6;
    //jpeg_dev->Instance->REG_JCR2.all = 0x3f;       // 0x808 bit[25:0]: Number of MCUs that will be encoded minus one.
    jpeg_dev->Instance->REG_JCR2.all = (pCfg->img_width_align/pCfg->sampling_h)*(pCfg->img_height_align/pCfg->sampling_v)-1;       // 0x808 bit[25:0]: Number of MCUs that will be encoded minus one.
    jpeg_dev->Instance->REG_JCR3.all = pCfg->rst_num ? (pCfg->rst_num-1) : 0;// 0x80C bit[15:0]: Number of MCUs between tow Restart Markers minus 1.
    //jpeg_dev->Instance->REG_JCR4.all = 0x30;       // 0x810 bit0:jcr_HD0  bit1:jcr_HA0  bit[3:2]:jcr_QT0  bit[7:4]:jcr_Nblock0
    //jpeg_dev->Instance->REG_JCR5.all = 0x7;        // 0x814 bit0:jcr_HD1  bit1:jcr_HA1  bit[3:2]:jcr_QT1  bit[7:4]:jcr_Nblock1
    //jpeg_dev->Instance->REG_JCR6.all = 0x7;        // 0x818 bit0:jcr_HD2  bit1:jcr_HA2  bit[3:2]:jcr_QT2  bit[7:4]:jcr_Nblock2
    jpeg_dev->Instance->REG_JCR7.all = 0x0;        // 0x81C bit0:jcr_HD3  bit1:jcr_HA3  bit[3:2]:jcr_QT3  bit[7:4]:jcr_Nblock3

    // 0x20 dec_dummy bit[1:0]= 00: Normal   01: 1 dummy byte   10: 2 dummy bytes   11: 3 dummy bytes
    // 0x40 int_st_clr W1C  bit3:edma_done   bit2:pdma_done   bit0:codec_done
    // 0x44 int_mask        bit3:edma_done   bit2:pdma_done   bit0:codec_done
    // 0x60 scaling_ctrl

    jpeg_dev->Instance->REG_INT_ST_CLR.all = (1 << 3) | (1 << 2) | (1 << 0);
    jpeg_dev->Instance->REG_INT_MASK.all = 0x0;

    jpeg_dev->cb_event = (Jpeg_SignalEvent_t)pCallback;

    /* Init the low level hardware and interrupt */
    register_ISR(IRQ_JPEG_VECTOR, (ISR)Jpeg_IRQ_Handler, NULL);
    clear_IRQ(IRQ_JPEG_VECTOR);
    enable_IRQ(IRQ_JPEG_VECTOR);

    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitialize JPEG device and release resources
 *        Cleanly disables all functionality and removes interrupt handlers
 *        @param[in] pJpegDev Pointer to device structure to uninitialize
 *        @return CSK_DRIVER_OK on success, error code on failure
 *        @note Should be called before system powerdown or reconfiguration
 */
int32_t Jpeg_Uninitialize(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Jpeg_Stop(jpeg_dev);

    Jpeg_Reset();

    memset(&jpeg_dev->Init, 0, sizeof(Jpeg_InitTypeDef));

    disable_IRQ(IRQ_JPEG_VECTOR);
    register_ISR(IRQ_JPEG_VECTOR, NULL, NULL);

    return CSK_DRIVER_OK;
}

/**
 * @brief Start JPEG processing operation
 *        Triggers hardware execution of configured encoding/decoding task
 *        @param[in] pJpegDev Pointer to initialized device structure
 *        @return CSK_DRIVER_OK on success, error code on failure
 *        @note Caller must ensure proper initialization was performed first
 */
int32_t Jpeg_Start(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_JCR0.bit.JCR_START_STOP = 0x1;             // 0x800 bit0: Start or stop the encoding or decoding process
    jpeg_dev->Instance->REG_RELOAD.bit.RELOAD = 0x1;                   // 0x70 bit0: reload  W1P
    jpeg_dev->Instance->REG_PIXEL_DMA_START.bit.PDMA_START = 0x1;      // 0x08 bit0: Write 1 to start pixel DMA transfer. (not used)
    jpeg_dev->Instance->REG_ENC_PDMA_START.bit.ARCS_PDMA_START = 0x1;  // 0x64 bit0: pixel dma start  W1P
    jpeg_dev->Instance->REG_ECS_DMA_START.bit.EDMA_START = 0x1;        // 0x0C bit0: Write 1 to start ECS DMA transfer.
    jpeg_dev->Instance->REG_IFCTRL.bit.JPEG_ENABLE= 0x1;               // 0x74 bit0: jpeg_enable

    /* Return function status */
    return CSK_DRIVER_OK;
}

/**
 * @brief Stop ongoing JPEG processing operation
 *        Halts current operation and disables hardware engine
 *        @param[in] pJpegDev Pointer to active device structure
 *        @return CSK_DRIVER_OK on success, error code on failure
 *        @note Safe to call even when no operation is currently running
 */
int32_t Jpeg_Stop(void *pJpegDev)
{
    Jpeg_DEV *jpeg_dev = safe_jpeg_dev(pJpegDev);

    /* Check the Jpeg instance */
    if(NULL == jpeg_dev)
    {
        DEV_LOG("[%s:%d] Error input: pJpegDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    jpeg_dev->Instance->REG_JCR0.bit.JCR_START_STOP = 0x0;             // 0x800 bit0: Start or stop the encoding or decoding process
    jpeg_dev->Instance->REG_IFCTRL.bit.JPEG_ENABLE= 0x0;               // 0x74 bit0: jpeg_enable

    return CSK_DRIVER_OK;
}
