#include <string.h>
#include <stdint.h>
#include "arcs_ap.h"
#include "blender.h"
#include "log_print.h"


#define DEBUG_LOG   1
#if DEBUG_LOG
#define DEV_LOG(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define DEV_LOG(format, ...)
#endif // DEBUG_LOG


//#define SYSCTRL_CFG         ((SYSCFG_RegDef*) CMN_SYSCTRL_BASE)

// driver version
#define CSK_BLENDER_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

static const
CSK_DRIVER_VERSION blender_driver_version = { CSK_BLENDER_API_VERSION, CSK_BLENDER_DRV_VERSION };

//------------------------------------------------------------------------------------------


_FAST_DATA_VI static Blender_DEV blender0_dev = {
        (IP_D2BLENDER),
        {0},
};


//------------------------------------------------------------------------------------

static void Blender_Reset(void)
{
    // reset the Blender module
    //__HAL_CRM_VIDEO_CLK_ENABLE();
    //IP_AP_CFG->REG_SW_RESET.bit.VIDEO_RESET = 0x1;
    //mmio_write32(IMAGE_PROC_BASE + 0xC, 1);   // d2bledner enable
    return;
}


static Blender_DEV * safe_blender_dev(void *blender_dev)
{
    //TODO: safe check of Blender device parameter
    if (blender_dev == Blender0()) {
        if (((Blender_DEV *)blender_dev)->Instance != (IP_D2BLENDER)) {
            DEV_LOG("[%s:%d] Error blender_dev=%#x ", __func__, __LINE__, blender_dev);
            return NULL;
        }
    } else {
        DEV_LOG("[%s:%d] Error blender_dev=%#x ", __func__, __LINE__, blender_dev);
        return NULL;
    }

    return (Blender_DEV *)blender_dev;
}


static uint8_t blender_get_backformat_byte(Blender_emBackFormat format)
{
    switch(format)
    {
        case BLENDER_BACK_FORMAT_RGB565:
            return 2;

        case BLENDER_BACK_FORMAT_RGB888:
            return 3;

        case BLENDER_BACK_FORMAT_ARGB8888:
            return 4;

        default:
            return 0;
    }
}


static uint8_t blender_get_foreformat_byte(Blender_emForeFormat format)
{
    switch(format)
    {
        case BLENDER_FORE_FORMAT_L8:
            return 1;

        case BLENDER_FORE_FORMAT_RGB565:
        case BLENDER_FORE_FORMAT_ARGB1555:
        case BLENDER_FORE_FORMAT_ARGB4444:
            return 2;

        case BLENDER_FORE_FORMAT_RGB888:
            return 3;

        case BLENDER_FORE_FORMAT_ARGB8888:
            return 4;

        default:
            return 0;
    }
}


static int32_t blender_check_parameter(Blender_InitTypeDef *pCfg)
{
    if((pCfg->blender_mode == 0) && (pCfg->alpha_mode == 0))
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->alpha_mode == 0)
    {
        if((pCfg->fore_format != BLENDER_FORE_FORMAT_ARGB8888) && \
           (pCfg->fore_format != BLENDER_FORE_FORMAT_ARGB1555) && \
           (pCfg->fore_format != BLENDER_FORE_FORMAT_ARGB4444))
        {
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    if(pCfg->blender_mode >= BLENDER_MODE_BUTT)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->alpha_mode >= BLENDER_ALPHA_MODE_BUTT)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->back_format >= BLENDER_BACK_FORMAT_BUTT)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->fore_format >= BLENDER_FORE_FORMAT_BUTT)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->img_width == 0)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(pCfg->img_height == 0)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if((pCfg->burst_thd != 1) && (pCfg->burst_thd != 2) && (pCfg->burst_thd != 4) && (pCfg->burst_thd != 8))
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return CSK_DRIVER_OK;
}


/* Exported functions --------------------------------------------------------*/


/**
  * @brief  Return Blender driver version.
  *
  * @return CSK_DRIVER_VERSION
  */
CSK_DRIVER_VERSION
Blender_GetVersion(void)
{
    return blender_driver_version;
}


/**
  * @brief  Return Blender instance.
  *
  * @return Instance of Blender
  */
void* Blender0(void)
{
    return &blender0_dev;
}


/**
 * @brief Initialize the Blender (Digital Video Processor) device.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @param pCfg The configuration structure for the Blender device.
 * @return int32_t Returns CSK_DRIVER_OK if initialization is successful, otherwise returns an error code.
 */
int32_t Blender_Initialize(void *pBlenderDev, Blender_InitTypeDef *pCfg)
{
    Blender_DEV *blender_dev = safe_blender_dev(pBlenderDev);

    /* Check the Blender instance */
    if(NULL == blender_dev)
    {
        DEV_LOG("[%s:%d] Error input: pBlenderDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(NULL == pCfg)
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if(CSK_DRIVER_OK != blender_check_parameter(pCfg))
    {
        DEV_LOG("[%s:%d] Error input: pCfg is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Blender_Stop(blender_dev);

    memcpy(&blender_dev->Init, pCfg, sizeof(Blender_InitTypeDef));

    blender_dev->Instance->REG_BLENDER_CTRL.bit.BLENDER_MODE = blender_dev->Init.blender_mode;           // 0x04
    blender_dev->Instance->REG_BLENDER_CTRL.bit.ALPHA_MODE = blender_dev->Init.alpha_mode;               // 0x04
    blender_dev->Instance->REG_BLENDER_CTRL.bit.BACK_COLOR_MODE = blender_dev->Init.back_format;         // 0x04
    blender_dev->Instance->REG_BLENDER_CTRL.bit.FORE_COLOR_MODE = blender_dev->Init.fore_format;         // 0x04
    blender_dev->Instance->REG_ALPHA.bit.ALPHA = blender_dev->Init.alpha;                                // 0x08
    blender_dev->Instance->REG_COLOR.all = blender_dev->Init.color;                                      // 0x0C

    if(blender_dev->Init.burst_thd >= BLENDER_FORE_BURST_LEN_MAX) {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2FORE_BURST_THD = BLENDER_FORE_BURST_LEN_MAX & 0x3F; // 0x10 bit[5:0]:
    }else {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2FORE_BURST_THD = blender_dev->Init.burst_thd & 0x3F; // 0x10 bit[5:0]:
    }

    if(blender_dev->Init.burst_thd >= BLENDER_BACK_BURST_LEN_MAX) {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2BACK_BURST_THD = BLENDER_BACK_BURST_LEN_MAX & 0x3F; // 0x10 bit[13:8]:
    }else {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2BACK_BURST_THD = blender_dev->Init.burst_thd & 0x3F; // 0x10 bit[13:8]:
    }

    if(blender_dev->Init.burst_thd >= BLENDER_OUT_BURST_LEN_MAX) {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2OUT_BURST_THD = BLENDER_OUT_BURST_LEN_MAX & 0x3F; // 0x10 bit[21:16]:
    }else {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2OUT_BURST_THD = blender_dev->Init.burst_thd & 0x3F; // 0x10 bit[21:16]:
    }

    if(blender_dev->Init.burst_thd >= BLENDER_MASK_BURST_LEN_MAX) {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2MASK_BURST_THD = BLENDER_MASK_BURST_LEN_MAX & 0x3F; // 0x10 bit[27:24]:
    }else {
        blender_dev->Instance->REG_FIFO_BURST_THD.bit.D2MASK_BURST_THD = blender_dev->Init.burst_thd & 0x3F; // 0x10 bit[27:24]:
    }

    blender_dev->Instance->REG_BLENDER_EN.bit.REG_CLK_FORCE_ON= 0x0;                      // 0x00 bit0: reg_clk_force_on

    blender_dev->Instance->REG_D2BLENDER_BACK_SIZE.bit.D2BLENDER_BACK_SIZE = blender_dev->Init.img_width * blender_dev->Init.img_height * blender_get_backformat_byte(blender_dev->Init.back_format) / sizeof(uint32_t);
    blender_dev->Instance->REG_D2BLENDER_FORE_SIZE.bit.D2BLENDER_FORE_SIZE = blender_dev->Init.img_width * blender_dev->Init.img_height * blender_get_foreformat_byte(blender_dev->Init.fore_format) / sizeof(uint32_t);
    blender_dev->Instance->REG_D2BLENDER_MASK_SIZE.bit.D2BLENDER_MASK_SIZE = blender_dev->Init.img_width * blender_dev->Init.img_height / sizeof(uint32_t);

    return CSK_DRIVER_OK;
}


/**
 * @brief Uninitializes the Blender device.
 *
 * This function uninitializes the Blender device by performing a reset and disabling the VI.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return CSK_DRIVER_OK if the operation is successful, otherwise returns an error code.
 */
int32_t Blender_Uninitialize(void *pBlenderDev)
{
    Blender_DEV *blender_dev = safe_blender_dev(pBlenderDev);

    /* Check the Blender instance */
    if(NULL == blender_dev)
    {
        DEV_LOG("[%s:%d] Error input: pBlenderDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    Blender_Stop(blender_dev);

    Blender_Reset();

    memset(&blender_dev->Init, 0, sizeof(Blender_InitTypeDef));

    return CSK_DRIVER_OK;
}


/**
 * @brief Start the Blender to capture video frames.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t Blender_Start(void *pBlenderDev)
{
    Blender_DEV *blender_dev = safe_blender_dev(pBlenderDev);

    /* Check the Blender instance */
    if(NULL == blender_dev)
    {
        DEV_LOG("[%s:%d] Error input: pBlenderDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    blender_dev->Instance->REG_BLENDER_EN.bit.BLENDER_EN= 0x1;

    /* Return function status */
    return CSK_DRIVER_OK;
}


/**
 * @brief Stops the Blender and releases its resources.
 *
 * @param pBlenderDev A pointer to the Blender device structure.
 * @return int32_t Returns CSK_DRIVER_OK if successful, otherwise returns an error code.
 */
int32_t Blender_Stop(void *pBlenderDev)
{
    Blender_DEV *blender_dev = safe_blender_dev(pBlenderDev);

    /* Check the Blender instance */
    if(NULL == blender_dev)
    {
        DEV_LOG("[%s:%d] Error input: pBlenderDev is NULL", __func__, __LINE__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    blender_dev->Instance->REG_BLENDER_EN.bit.BLENDER_EN= 0x0;

    return CSK_DRIVER_OK;
}



