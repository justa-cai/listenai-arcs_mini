/**
 * @file adc_pdm.c
 * @brief ADC/PDM driver implementation for audio input interface
 * @version 1.0
 * @date 2023-05-15
 * @copyright Copyright (c) 2023
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "venusa_ap.h"
#include "Driver_ADC_PDM.h"
#include "adc_pdm.h"
#include "ClockManager.h" // for CRM_GetSrcFreq()

#include "log_print.h"

#define DEBUG_LOG   1 //0 //
#if DEBUG_LOG
#define LOGD(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define ADC_PDM_USE_PIO     0 //1 //RX FIFO -> RAM, 1: use PIO, 0: use DMA (default)
#define DAC_USE_GPDMA2D     (!USE_CMNDMA & USE_GPDMA2D) // 1: use GPDMA2D, 0: use CMNDMA
#define CSK_ADC_PDM_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

/**
 * @brief Driver version structure
 */
static const
CSK_DRIVER_VERSION adc_pdm_driver_version = { CSK_ADC_PDM_API_VERSION, CSK_ADC_PDM_DRV_VERSION };

static void adc_pdm_apc_event(uint32_t event_info, uint32_t usr_param);

//------------------------------------------------------------------------------------------
/**
 * @brief Global system control register definition
 */
static CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;

/**
 * @brief Global audio codec register definition
 */
AUDIO_CODEC_RegDef *g_codec = (AUDIO_CODEC_RegDef *)CODEC_BASE;

// ADC_PDM01
_FAST_DATA_VI static ADC_PDM_INFO adc01_info = { 0 };
_FAST_DATA_VI static ADC_PDM_GRP adc01_grp = {
        0, APC_DCH_ADC01,
        CSK_ADC_PDM_SAMPLE_BITS, 0,
        CSK_ADC01,
        &adc01_info
};

/**
 * @brief Get ADC/PDM01 device group instance
 * @return Pointer to ADC/PDM01 group instance
 */
void* ADC_PDM01() { return &adc01_grp; }

//------------------------------------------------------------------------------------------
/**
 * @brief Safely verify and get ADC/PDM group pointer
 * @param adc_pdm_grp Pointer to ADC/PDM group
 * @return Validated ADC/PDM group pointer or NULL
 */
static ADC_PDM_GRP * safe_adc_pdm_grp(void *adc_pdm_grp)
{
    // safe check of ADC/PDM device parameter
    if (adc_pdm_grp == &adc01_grp) {
        if (adc01_grp.reg != CSK_ADC01 || adc01_grp.apc_dch != APC_DCH_ADC01 || adc01_grp.dev_idx != 0) {
            CLOGW("ADC/PDM device context has been tampered illegally!!\n");
            return NULL;
        }
    } else {
        return NULL;
    }

    return (ADC_PDM_GRP *)adc_pdm_grp;
}

/**
 * @brief Enable ADC/PDM group (digital & analog) and its internal clock
 * @param adc Pointer to ADC/PDM group
 * @param dev_bmp Device bitmap
 */
static void enable_adc_pdm(ADC_PDM_GRP *adc, uint8_t dev_bmp)
{
    uint32_t reg_val, reg_org, val;
    assert(adc != NULL);

    if (adc->info->use_pdm) { // DMIC
        // enable DMIC
        reg_val = adc->reg->REG_AUD_ADC_CTRL4.all;
        if (!(reg_val & DMIC_EN))
            adc->reg->REG_AUD_ADC_CTRL4.all = reg_val | DMIC_EN;

    } else { // AMIC
        // disable DMIC
        reg_val = adc->reg->REG_AUD_ADC_CTRL4.all;
        if (reg_val & DMIC_EN)
            adc->reg->REG_AUD_ADC_CTRL4.all = reg_val & ~DMIC_EN;

        // ADC-specific analog settings
        reg_org = reg_val = adc->reg->REG_AUD_ADC_CTRL6.all;

        val = dev_bmp & ADC_PDM_BMP_STEREO;
        if (val == ADC_PDM_BMP_STEREO)
            reg_val |= ADCLR_EN; // enable ADC L/R
        else if (val == ADC_PDM_BMP_LEFT)
            reg_val |= ADCL_EN; // enable ADC Left
        else if (val == ADC_PDM_BMP_RIGHT)
            reg_val |= ADCR_EN; // enable ADC Right
        else {
            CLOGW("%s: invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
            return;
        }
        if (reg_val != reg_org)
            adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;

/*
        // it's a workaround to make sure RCCTRL(r5) is set to 12MHz after CALI_GO is done!
        if (adc->info->rc_ctrl != 0)
            adc->reg->REG_AUD_ADC_CTRL7.bit.AUD_ADC_MODE = 1; //12MHz
            //adc->reg->REG_AUD_ADC_CTRL7.all |= ADC_MODE_12MHZ;
        else
            adc->reg->REG_AUD_ADC_CTRL7.bit.AUD_ADC_MODE = 0; //4MHz
            //adc->reg->REG_AUD_ADC_CTRL7.all &= ~ADC_MODE_12MHZ;
*/
    }

    // enable ADC internal clock
    reg_val = adc->reg->REG_AUD_ADC_CTRL0.all;
    if (!(reg_val & ADCCLK_EN)) {
        adc->reg->REG_AUD_ADC_CTRL0.all = reg_val | ADCCLK_EN;
    }
}

/**
 * @brief Disable ADC/PDM group (digital & analog) and its internal clock
 * @param adc Pointer to ADC/PDM group
 */
static void disable_adc_pdm(ADC_PDM_GRP *adc) // for both L&R channels
{
    uint32_t reg_val;
    assert(adc != NULL);

    if (adc->info->use_pdm) { // DMIC
        // disable DMIC
        adc->reg->REG_AUD_ADC_CTRL4.all &= ~DMIC_EN;
    } else { // AMIC
        // disable ADC L/R
        adc->reg->REG_AUD_ADC_CTRL6.all &= ~ADCLR_EN;
    }

    reg_val = adc->reg->REG_AUD_ADC_CTRL0.all;
    if (reg_val & ADCCLK_EN) {
        // disable ADC' all internal clocks
        adc->reg->REG_AUD_ADC_CTRL0.all = reg_val & ~ADCCLK_EN;
    }
}

/**
 * @brief Enable ADC/PDM group's external clock
 * @param adc Pointer to ADC/PDM group
 */
static inline void enable_adc_pdm_clk(ADC_PDM_GRP *adc)
{
    codec_clk_enable();
}

/**
 * @brief Disable ADC/PDM group's external clock
 * @param adc Pointer to ADC/PDM group
 */
static inline void disable_adc_pdm_clk(ADC_PDM_GRP *adc)
{
    codec_clk_disable();
}

/**
 * @brief Set ADC to low power mode
 * @param adc Pointer to ADC/PDM group
 */
static inline void set_adc_low_power(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);
    uint32_t reg_val, reg_org;

    //R11 rc_ctrl & ib_ctrl
    reg_val = adc->reg->REG_AUD_ADC_CTRL6.all;
    //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
    //NOTE: DON'T change IB_CTRL when 12MHz currently...
    if (adc->info->rc_ctrl == 0) { // 4MHz
        reg_val &= ~ADC_IB_CTRL_MASK;
        reg_val |= ADC_IB_CTRL(IB_CTRL_VAL_LP);
        adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;
    }

    //R12 low power bits
    reg_org = reg_val = adc->reg->REG_AUD_ADC_CTRL7.all;
    // INT1_LP & INT2_LP are default settings
    reg_val |= PGA_VCOM_SEL_VMID_75PER | ADC_SAR_COMP_LP | PGA_LP |
            ADC_INT2_LP | ADC_INT1_LP;
    if (reg_val != reg_org)
        adc->reg->REG_AUD_ADC_CTRL7.all = reg_val;
}

/**
 * @brief Clear ADC low power mode
 * @param adc Pointer to ADC/PDM group
 */
static inline void clear_adc_low_power(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);
    uint32_t reg_val, reg_org;

    //R12 low power bits
    reg_org = reg_val = adc->reg->REG_AUD_ADC_CTRL7.all;
    reg_val &= ~(PGA_VCOM_SEL_VMID_75PER | PGA_LP);
    if (reg_val != reg_org)
        adc->reg->REG_AUD_ADC_CTRL7.all = reg_val;

    //R11 rc_ctrl & ib_ctrl
    reg_val = adc->reg->REG_AUD_ADC_CTRL6.all;
    //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
    //NOTE: DON'T change IB_CTRL when 12MHz currently...
    if (adc->info->rc_ctrl == 0) { // 4MHz
        reg_val &= ~ADC_IB_CTRL_MASK;
        reg_val |= ADC_IB_CTRL(IB_CTRL_VAL_DEF);
        adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;
    }
}

//------------------------------------------------------------------------------------------

/**
 * @brief Get driver version
 * @return Driver version information
 */
CSK_DRIVER_VERSION
ADC_PDM_GetVersion()
{
    return adc_pdm_driver_version;
}

/**
 * @brief Initialize ADC/PDM device group interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param cb_event Pointer to event callback function
 * @param usr_param User-defined parameter for callback
 * @param dev_bmp_flag Device bitmap flag
 * @param dma_chs_p Pointer to DMA channels structure
 * @return Execution status
 */
int32_t
ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
                uint32_t dev_bmp_flag, ADC_PDM_DMA_CHS *dma_chs_p)
{
    g_sysctrl = IP_SYSCTRL;
    g_codec = (AUDIO_CODEC_RegDef *)CODEC_BASE;

    int32_t ret;
    uint8_t dev_bmp = (dev_bmp_flag & ADC_PDM_BMP_FLAG_IN_STEREO);
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);

    if (adc == NULL || dma_chs_p == NULL) {
        CLOGW("%s: invalid ADC/PDM device (0x%08x) or NULL DMA_CHS!", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (dev_bmp == 0) {
        CLOGW("%s: no valid channel (0x%x)!", __func__, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (adc->info->flags & ADC_PDM_FLAG_INITIALIZED)
        return CSK_ADCPDM_ERROR_INITED_ALREADY;

    // initialize APC
    ret = apc_initialize();
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: failed to call apc_initialize()!", __func__);
        return ret;
    }

    // acquire APC channels
    ret = apc_dual_channel_acquire(adc->apc_dch, APC_INTF_ADC_PDM, adc->dev_idx,
                                    (uint8_t *)dma_chs_p, adc_pdm_apc_event, (uint32_t)adc);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: failed to acquire APC channels!", __func__);
        return ret;
    }

    // initialize ADM/PDM run-time resources
    memset(adc->info, 0, sizeof(ADC_PDM_INFO));
    adc->info->cb_event = cb_event;
    adc->info->usr_param = usr_param;
    adc->info->use_pdm = (dev_bmp_flag & ADC_PDM_BMP_FLAG_USE_PDM) ? 1 : 0;
    adc->info->use_16bits = (dev_bmp_flag & ADC_PDM_BMP_FLAG_USE_16BITS) ? 1 : 0;
    adc->info->ch_bmp = dev_bmp;
    adc->info->status.all = 0U;

    //TODO: other initialization...

    adc->info->flags = ADC_PDM_FLAG_INITIALIZED; // ADM/PDM is initialized
    LOGD("%s: ADM/PDM version: API = 0x%x, DRV = 0x%x\n",
            __func__, adc_pdm_driver_version.api, adc_pdm_driver_version.drv);

    return CSK_DRIVER_OK;
}

/**
 * @brief De-initialize ADC/PDM device group interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @return Execution status
 */
int32_t
ADC_PDM_Uninitialize(void *adc_pdm_grp)
{
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    if (adc == NULL) {
        CLOGW("%s: invalid ADC/PDM device (0x%08x)!", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((adc->info->flags & ADC_PDM_FLAG_INITIALIZED) == 0)
        return CSK_DRIVER_ERROR;

    // Abort current tranfers if any
    //ADM_PDM_Abort_Channels(adc_pdm_grp, adc->info->ch_bmp);

    // Power off ADC/PDM if Powered on
    if (adc->info->flags & ADC_PDM_FLAG_POWERED)
        ADC_PDM_PowerControl(adc_pdm_grp, CSK_POWER_OFF);

    // Release APC channels
    assert(adc->info->ch_bmp != 0);
    apc_dual_channel_release(adc->apc_dch);

    // Uninitialize APC
    apc_uninitialize();

    adc->info->flags = 0U; // ADC/PDM is uninitialized
    LOGD("%s is called for ADC/PDM couple %d!\n", __func__, adc->dev_idx);
    return CSK_DRIVER_OK;
}

/**
 * @brief Controls power state of ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param state Requested power state (OFF/LOW/FULL)
 * @return int32_t CSK_DRIVER_OK on success, error code on failure
 *
 * @note All power operations require the device to be initialized first.
 *       Uninitialized devices will return CSK_DRIVER_ERROR.
 */
int32_t ADC_PDM_PowerControl(void *adc_pdm_grp, CSK_POWER_STATE state)
{
    // 1. Parameter Validation
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    if (adc == NULL) {
        CLOGW("Invalid ADC/PDM device (0x%08x)", adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // 2. State Pre-Check
    const bool is_initialized = (adc->info->flags & ADC_PDM_FLAG_INITIALIZED);
    const bool is_powered = (adc->info->flags & ADC_PDM_FLAG_POWERED);
    const bool is_low_power = (adc->info->flags & ADC_PDM_FLAG_LOW_POWER);

    // 3. Strict Initialization Check for ALL states
    if (!is_initialized) {
        CLOGE("Operation rejected: ADC/PDM not initialized");
        return CSK_DRIVER_ERROR;
    }

    // 4. State-Specific Handling
    switch (state) {
    case CSK_POWER_OFF:
        if (is_powered) {
            ADC_PDM_Abort(adc_pdm_grp, adc->info->ch_bmp);
            disable_adc_pdm(adc);
            //disable_adc_pdm_clk(adc); //FIXME: ADC & DAC share same clock gate
            adc->info->flags &= ~(ADC_PDM_FLAG_POWERED | ADC_PDM_FLAG_LOW_POWER | ADC_PDM_FLAG_CONFIGURED);
        }
        break;

    case CSK_POWER_LOW:
        if (is_powered) {
            if (!is_low_power) {
                set_adc_low_power(adc);
                adc->info->flags |= ADC_PDM_FLAG_LOW_POWER;
            }
        } else {
            enable_adc_pdm_clk(adc);
            set_adc_low_power(adc);
            adc->info->flags |= (ADC_PDM_FLAG_POWERED | ADC_PDM_FLAG_LOW_POWER);
        }
        break;

    case CSK_POWER_FULL:
        if (is_powered) {
            if (is_low_power) {
                clear_adc_low_power(adc);
                adc->info->flags &= ~ADC_PDM_FLAG_LOW_POWER;
            }
        } else {
            enable_adc_pdm_clk(adc);
            adc->info->flags |= ADC_PDM_FLAG_POWERED;
            adc->info->flags &= ~ADC_PDM_FLAG_LOW_POWER;
        }
        break;

    default:
        CLOGW("Unsupported power state: %d", state);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Enable RX channels for ADC/PDM
 * @param adc Pointer to ADC/PDM group
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
static int32_t adc_enable_rx_channels(ADC_PDM_GRP *adc, uint8_t dev_bmp)
{
    uint8_t bmp;
    int32_t ret0 = CSK_DRIVER_OK;

    assert(adc != NULL);
    bmp = (dev_bmp & adc->info->ch_bmp);
    if (bmp != 0) { // IN channels
        ret0 = CSK_DRIVER_ERROR;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret0 = apc_dual_channel_enable(adc->apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            ret0 = apc_channel_enable(APC_DCH_TO_CH(adc->apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret0 = apc_channel_enable(APC_DCH_TO_CH(adc->apc_dch, 1));
        } else {
            assert(0);
        }
    }

    return ret0;
}

/**
 * @brief Disable RX channels for ADC/PDM
 * @param adc Pointer to ADC/PDM group
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
static int32_t adc_disable_rx_channels(ADC_PDM_GRP *adc, uint8_t dev_bmp)
{
    uint8_t bmp;
    int32_t ret0 = CSK_DRIVER_OK;

    assert(adc != NULL);
    bmp = (dev_bmp & adc->info->ch_bmp);
    if (bmp != 0) { // IN channels
        ret0 = CSK_DRIVER_ERROR;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret0 = apc_dual_channel_disable(adc->apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            ret0 = apc_channel_disable(APC_DCH_TO_CH(adc->apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret0 = apc_channel_disable(APC_DCH_TO_CH(adc->apc_dch, 1));
        } else {
            assert(0);
        }
    }

    return ret0;
}

/**
 * @brief Receive data from ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param data Pointer to buffer to receive data
 * @param num Number of data items to receive
 * @param dev_bmp Device bitmap
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t
ADC_PDM_Receive(void *adc_pdm_grp, uint32_t *data, uint32_t num,
                uint8_t dev_bmp, uint8_t rx_flag)
{
    int32_t ret;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;
    uint8_t full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

    if (full_chk) {
    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        LOGD("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        LOGD("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }
    } // full check

    if (adc->info->status.bit.busy & bmp)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t start_now = rx_flag & ADC_PDM_RX_FLAG_START_NOW;
    if (!start_now) {
        ret = adc_disable_rx_channels(adc, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    ret = CSK_DRIVER_ERROR;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read(adc->apc_dch, data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read(APC_DCH_TO_CH(adc->apc_dch, 0), data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read(APC_DCH_TO_CH(adc->apc_dch, 1), data, num, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    adc->info->status.bit.busy |= bmp;
    adc->info->status.bit.rx_full = 0;
    adc->info->status.bit.rx_ovf = 0;

    if (start_now) {
        //enable_adc_pdm(adc, bmp); // enable ADC/PDM module
        ret = adc_enable_rx_channels(adc, bmp);
    }

    return ret;
}

/**
 * @brief Receive data via ADC/DMIC interface in Ping/Pong mode
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param blks Pointer to PIPO_IN_BLOCK structures
 * @param blk_cnt_p Pointer to block count
 * @param dev_bmp Device bitmap
 * @param rx_flag Receive operation flags
 * @return Execution status
 */
int32_t
ADC_PDM_Receive_PiPo(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t dev_bmp, uint8_t rx_flag)
{
    int32_t ret;
    uint8_t bmp = 0;
    //uint8_t full_chk = !(tx_flag & ADC_PDM_RX_FLAG_QUICK_CHECK);

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    // || blks == NULL || blk_cnt_p == NULL
    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        LOGD("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t started = adc->info->status.bit.busy;
    uint8_t start_now = 0;

    if (!started) {
        if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
            LOGD("%s: ADC/PDM couple %d has NOT been configured!!",
                    __func__, adc->dev_idx);
            return CSK_DRIVER_ERROR;
        }

        //if (adc->info->status.bit.busy & bmp)
        //    return CSK_DRIVER_ERROR_BUSY;

        start_now = rx_flag & ADC_PDM_RX_FLAG_START_NOW;
        if (!start_now) {
            ret = adc_disable_rx_channels(adc, bmp);
            if (ret != CSK_DRIVER_OK)
                return ret;
        }
    } // !started

    ret = apc_dch_read_pipo(adc->apc_dch, dev_bmp, blks, blk_cnt_p, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        adc->info->status.bit.busy |= bmp;
        adc->info->status.bit.rx_full = 0;
        adc->info->status.bit.rx_ovf = 0;

        if (start_now) {
            //enable_adc_pdm(adc, bmp); // enable ADC/PDM module
            ret = adc_enable_rx_channels(adc, bmp);
        }
    }

    return ret;
}

/**
 * @brief Get transferred blocks count for Ping/Pong mode
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param blks Pointer to PIPO_IN_BLOCK structures
 * @param blk_cnt Block count
 * @param dev_bmp Device bitmap
 * @return Count of transferred blocks or error value
 */
int32_t
ADC_PDM_PiPo_Xferred_Blocks(void *adc_pdm_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp)
{
    int32_t ret;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    // blks == NULL || blk_cnt == 0
    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) || !adc->info->status.bit.busy) {
        CLOGW("%s: ADC/PDM has NOT been configured or started!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    return apc_dch_get_pipo_blks(adc->apc_dch, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
}

/**
 * @brief Enable ADC/PDM interface and start data receiving
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
int32_t
ADC_PDM_Enable(void *adc_pdm_grp, uint8_t dev_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    ret = adc_enable_rx_channels(adc, bmp);
    if (ret == CSK_DRIVER_OK)
        enable_adc_pdm(adc, bmp); // enable ADC/PDM module

    return ret;
}

/**
 * @brief Disable ADC/PDM interface (suspend data receiving)
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
int32_t
ADC_PDM_Disable(void *adc_pdm_grp, uint8_t dev_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    ret = adc_disable_rx_channels(adc, bmp);
    return ret;
}

/**
 * @brief Abort ADC/PDM data transfer
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
int32_t
ADC_PDM_Abort(void *adc_pdm_grp, uint8_t dev_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (adc->info->ch_mix || bmp == CH_BMP_STEREO)
        ret = apc_dual_channel_abort(adc->apc_dch);
    else if (bmp == CH_BMP_LEFT)
        ret = apc_channel_abort(APC_DCH_TO_CH(adc->apc_dch, 0));
    else if (bmp == CH_BMP_RIGHT)
        ret = apc_channel_abort(APC_DCH_TO_CH(adc->apc_dch, 1));
    if (ret != CSK_DRIVER_OK)
        return ret;

    adc->info->status.bit.busy &= ~bmp;
    return ret;
}

/**
 * @brief Get received data count from ADC/PDM instance
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param dev_bmp Device bitmap
 * @return Number of data items transferred or error value
 */
int32_t
ADC_PDM_GetRxCount(void *adc_pdm_grp, uint8_t dev_bmp)
{
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (adc->info->ch_mix)
            return apc_dual_channel_get_count(adc->apc_dch);

        int32_t ret = 0;
        if (adc->info->ch_bmp & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(adc->apc_dch, 0));
        if (adc->info->ch_bmp & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(adc->apc_dch, 1));
        return ret;

    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(adc->apc_dch, 0));
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(adc->apc_dch, 1));

    return CSK_DRIVER_ERROR;
}

/**
 * @brief Get current sample rate
 * @param adc Pointer to ADC/PDM group
 * @return Sample rate or error value
 */
static int32_t adc_get_samp_rate(ADC_PDM_GRP *adc)
{
    uint32_t i, reg_val;
    assert(adc != NULL);

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (adc->info->samp_freq != 0)
        return adc->info->samp_freq;

    reg_val = adc->reg->REG_AUD_ADC_CTRL0.bit.ADCSR;
    for (i = 0; i < ARRAY_COUNT(sc_ADC_SR_val_map); i++) {
        if (reg_val == sc_ADC_SR_val_map[i].reg_val)
            break;
    }
    if (i >= ARRAY_COUNT(sc_ADC_SR_val_map)) {
        CLOGW("%s: Sample rate's reg_val (%d) is NOT in built-in supported freq list!!\r\n",
                __func__, reg_val);
        return CSK_DRIVER_ERROR;
    }

    return sc_ADC_SR_val_map[reg_val].real_val;
}

/**
 * @brief Set ALC (Automatic Level Control) parameters
 * @param adc Pointer to ADC/PDM group
 * @param alc_p Pointer to ALC parameters
 * @return Execution status
 */
static uint32_t adc_set_alc_params(ADC_PDM_GRP *adc, ALC_PARAMS *alc_p)
{
    union AUD_ADC_CTRL3 reg_ctrl3;
    union AUD_ADC_CTRL4 reg_ctrl4;
    uint8_t set_ctrl4 = 0;
    assert(adc != NULL && alc_p != NULL);

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    reg_ctrl3.all = adc->reg->REG_AUD_ADC_CTRL3.all;
    reg_ctrl4.all = adc->reg->REG_AUD_ADC_CTRL4.all;

    if (alc_p->alc_flags & ALC_FLAG_ALC_SEL_L)
    	reg_ctrl3.bit.ALCSEL_L = alc_p->alc_sel_l;
    if (alc_p->alc_flags & ALC_FLAG_ALC_SEL_R)
    	reg_ctrl3.bit.ALCSEL_R = alc_p->alc_sel_r;
    if (alc_p->alc_flags & ALC_FLAG_ERR_TOLERANCE) {
        if (alc_p->err_tolerance > ERR_TOLERANCE_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.TOLERANCE = alc_p->err_tolerance;
    }
    if (alc_p->alc_flags & ALC_FLAG_TARGET_L) {
        if (alc_p->target_l > TARGET_LEVEL_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.TARGET_L = alc_p->target_l;
    }
    if (alc_p->alc_flags & ALC_FLAG_TARGET_R) {
        if (alc_p->target_r > TARGET_LEVEL_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.TARGET_R = alc_p->target_r;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_MODE)
    	reg_ctrl3.bit.ALCMODE = alc_p->alc_mode;
    if (alc_p->alc_flags & ALC_FLAG_NGATE_EN)
    	reg_ctrl3.bit.NG_EN = alc_p->ngate_en;
    if (alc_p->alc_flags & ALC_FLAG_NGATE_FLOOR) {
        if (alc_p->ngate_floor > NGATE_FLOOR_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.NG = alc_p->ngate_floor;
    }

    if (alc_p->alc_flags & ALC_FLAG_ALC_MIN) {
        if (alc_p->alc_min > ALCMIN_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.ALCMIN = alc_p->alc_min;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_MAX) {
        if (alc_p->alc_max > ALCMAX_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl3.bit.ALCMAX = alc_p->alc_max;
    }
    if (alc_p->alc_flags & (ALC_FLAG_ALC_MIN | ALC_FLAG_ALC_MAX)) {
        if (reg_ctrl3.bit.ALCMIN > reg_ctrl3.bit.ALCMAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
    }

    if (alc_p->alc_flags & ALC_FLAG_ALC_HOLD) {
        if (alc_p->alc_hold > ALC_HOLD_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl4.bit.ALCHLD = alc_p->alc_hold;
        set_ctrl4 = 1;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_ATTACK) {
        if (alc_p->alc_attack > ALC_ATTACK_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl4.bit.ALCATK = alc_p->alc_attack;
        set_ctrl4 = 1;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_DECAY) {
        if (alc_p->alc_decay > ALC_DECAY_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_ctrl4.bit.ALCDCY = alc_p->alc_decay;
        set_ctrl4 = 1;
    }

    adc->reg->REG_AUD_ADC_CTRL3.all = reg_ctrl3.all;
    if (set_ctrl4)
        adc->reg->REG_AUD_ADC_CTRL4.all = reg_ctrl4.all;

    return CSK_DRIVER_OK;
}

/**
 * @brief Perform ADC calibration
 * @param adc Pointer to ADC/PDM group
 * @return Execution status
 */
static uint32_t adc_do_cali(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    // clear and then set ADC_CAP_CALI_GO bit
    adc->reg->REG_AUD_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 0x0; // clear it
    adc->reg->REG_AUD_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 0x1; // set it to start

    // it takes dozens of us to complete the calibration process...
    volatile uint32_t count = 300 * 10; //FIXME: 300 * 40 or bigger?
    while (count-- > 0);

    return CSK_DRIVER_OK;
}

/**
 * @brief Check if sample rate and oversample ratio pair is valid
 * @param adc Pointer to ADC/PDM group
 * @return true if pair is valid, false otherwise
 */
static bool check_sr_osr_pair(ADC_PDM_GRP *adc)
{
    uint32_t i, count;
    assert(adc != NULL);
    if (adc->info->use_pdm) { // DMIC
        count = ARRAY_COUNT(sc_DMIC_SR_OSR);
        for (i=0; i<count; i++) {
            if (sc_DMIC_SR_OSR[i].major == adc->info->samp_freq &&
                sc_DMIC_SR_OSR[i].minor == adc->info->over_samp_ratio) {
                //adc->info->rc_ctrl = sc_DMIC_SR_OSR[i].least & 0x1;
                return true;
            }
        } // end for

    } else { // AMIC
        count = ARRAY_COUNT(sc_AMIC_SR_OSR);
        for (i=0; i<count; i++) {
            if (sc_AMIC_SR_OSR[i].major == adc->info->samp_freq &&
                sc_AMIC_SR_OSR[i].minor == adc->info->over_samp_ratio) {
                adc->info->rc_ctrl = sc_AMIC_SR_OSR[i].least & 0x1;

                // ADC-specific analog settings
                uint32_t reg_val;
                reg_val = adc->reg->REG_AUD_ADC_CTRL6.all;
                reg_val &= ~ADC_IB_CTRL_MASK;

                //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
                //if (adc->info->rc_ctrl != 0 && !(adc->info->flags & ADC_PDM_FLAG_LOW_POWER))
                if (adc->info->rc_ctrl != 0) {
                    reg_val |=ADC_IB_CTRL(IB_CTRL_VAL_12MHZ); //12MHz, 2.5uA
                    adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;
                    adc->reg->REG_AUD_ADC_CTRL7.all |= ADC_MODE_12MHZ; // 12MHz
                } else {
                    reg_val |= ADC_IB_CTRL(IB_CTRL_VAL_DEF); //2.0uA
                    adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;
                    adc->reg->REG_AUD_ADC_CTRL7.all &= ~ADC_MODE_12MHZ; // 4MHz
                }

                return true;
            }
        } // end for

    }
    return false;
}

/**
 * @brief Control ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param control Operation to perform
 * @param arg Argument for operation
 * @return Execution status
 */
int32_t
ADC_PDM_Control(void *adc_pdm_grp, uint32_t control, uint32_t arg)
{
    int32_t ret;
    uint32_t val, reg_val;
    union AUD_ADC_CTRL0 reg_ctrl0;
    union AUD_ADC_CTRL1 reg_ctrl1;
    union AUD_ADC_CTRL4 reg_ctrl4;
    union AUD_ADC_CTRL7 reg_ctrl7;

    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    if (adc == NULL) {
        CLOGW("%s: invalid ADC/PDM device (0x%08x)!", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_POWERED))
        return CSK_DRIVER_ERROR;

    // exclusive MISC OP
    switch (control & CSK_ADCPDM_EXCL_OP_Msk) {
        // NO MISC OP
        case CSK_ADCPDM_EXCL_OP_UNSET:
            break;

        // abort ADC/PDM transfer
        case CSK_ADCPDM_ABORT_TRANSFER:
            ADC_PDM_Abort(adc, (arg & 0x3));
            return CSK_DRIVER_OK;

        // get sample rate
        case CSK_ADCPDM_GET_SAMP_RATE:
            return adc_get_samp_rate(adc);

        // set ALC parameters
        case CSK_ADCPDM_SET_ALC_PARAMS:
            if (adc->info->status.bit.busy)
                return CSK_DRIVER_ERROR_BUSY;
            return adc_set_alc_params(adc, (ALC_PARAMS*)arg);

        // start ADC CAP calibration process
        case CSK_ADCPDM_DO_CALIBRATION:
            if (adc->info->status.bit.busy)
                return CSK_DRIVER_ERROR_BUSY;
            return adc_do_cali(adc);

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    } // end exclusive MISC_OP

    // HPF (High Pass Filter) can be set when ADC is busy recording...
    if ((control & CSK_ADCPDM_HPF_Msk) == CSK_ADCPDM_HPF_SET) {
        reg_ctrl1.all = adc->reg->REG_AUD_ADC_CTRL1.all;
        reg_ctrl1.bit.HPF1EN = arg & 0x1; // bit[0]
        reg_ctrl1.bit.HPF2EN = (arg >> 1) & 0x1; // bit[1]
        reg_ctrl1.bit.HPFCUT = (arg >> 2) & 0x7; // bit[4:2]
        adc->reg->REG_AUD_ADC_CTRL1.all = reg_ctrl1.all;
        // remove HPF setting bits after done and just return if no other setting
        control &= ~CSK_ADCPDM_HPF_Msk;
        if (control == 0)
            return CSK_DRIVER_OK;
    }

    if (adc->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    reg_ctrl0.all = adc->reg->REG_AUD_ADC_CTRL0.all;
    reg_ctrl1.all = adc->reg->REG_AUD_ADC_CTRL1.all;
    reg_ctrl4.all = adc->reg->REG_AUD_ADC_CTRL4.all;
    reg_ctrl7.all = adc->reg->REG_AUD_ADC_CTRL7.all;


    // Sample Rate
    switch (control & CSK_ADCPDM_SR_Msk) {
    // Keep Sample Rate unchanged
    case CSK_ADCPDM_SR_UNSET:
        break;

    case CSK_ADCPDM_SR_8KHZ:
    	reg_ctrl0.bit.ADCSR = 0x0;
        adc->info->samp_freq = 8000;
        break;

    case CSK_ADCPDM_SR_16KHZ:
    	reg_ctrl0.bit.ADCSR = 0x3;
        adc->info->samp_freq = 16000;
        break;

    case CSK_ADCPDM_SR_48KHZ:
    	reg_ctrl0.bit.ADCSR = 0x8;
        adc->info->samp_freq = 48000;
        break;

    default:
        return CSK_ADCPDM_ERROR_SAMP_RATE;
    }

    // Over Sample Ratio
    switch (control & CSK_ADCPDM_OSR_Msk) {
    // Keep Over Sample Ratio unchanged
    case CSK_ADCPDM_OSR_UNSET:
        break;

    case CSK_ADCPDM_OSR_500:
    	reg_ctrl0.bit.ADCOSR = 0x0;
        adc->info->over_samp_ratio = 500;
        break;

    case CSK_ADCPDM_OSR_250:
    	reg_ctrl0.bit.ADCOSR = 0x1;
        adc->info->over_samp_ratio = 250;
        break;

    case CSK_ADCPDM_OSR_125:
    	reg_ctrl0.bit.ADCOSR = 0x2;
        adc->info->over_samp_ratio = 125;
        break;

    case CSK_ADCPDM_OSR_100:
    	reg_ctrl0.bit.ADCOSR = 0x3;
        adc->info->over_samp_ratio = 100;
        break;

    case CSK_ADCPDM_OSR_50:
    	reg_ctrl0.bit.ADCOSR = 0x4;
        adc->info->over_samp_ratio = 50;
        break;

    default:
        return CSK_ADCPDM_ERROR_SAMP_RATE;
    }

    // check if the combination of Sample Rate & Over Sample Ratio are supported
    if ((control & CSK_ADCPDM_SR_Msk) != CSK_ADCPDM_SR_UNSET ||
        (control & CSK_ADCPDM_OSR_Msk) != CSK_ADCPDM_OSR_UNSET) {
        if (!check_sr_osr_pair(adc)) {
            CLOGW("%s: unsupported pair of Sample Rate(%d) and Over Sample Ratio(%d)!\n",
                    __func__, adc->info->samp_freq, adc->info->over_samp_ratio);
            return CSK_ADCPDM_ERROR_SR_OSR_PAIR;
        }
    }

    // IN Channel Data Mix
    switch (control & CSK_ADCPDM_RXCFG_Msk) {
    // Keep channel data mix unchanged
    case CSK_ADCPDM_RXCFG_UNSET:
        break;

    case CSK_ADCPDM_RXCFG_SEPA:
        adc->info->ch_mix = 0;
         break;

    case CSK_ADCPDM_RXCFG_MIXED:
        adc->info->ch_mix = 1;
        break;

    default:
        return CSK_ADCPDM_ERROR_RXCFG;
    }

    // Latch delay
    val = control & CSK_ADCPDM_LATCH_DELAY_Msk;
    switch (val) {
    // Keep unchanged or default state
    case CSK_ADCPDM_LATCH_DELAY_UNSET:
        break;

    case CSK_ADCPDM_LATCH_DELAY_DGR0:
    case CSK_ADCPDM_LATCH_DELAY_DGR90:
    case CSK_ADCPDM_LATCH_DELAY_DGR180:
    case CSK_ADCPDM_LATCH_DELAY_DGR270:
        reg_ctrl4.bit.DMIC_LATCH_ADJ = (val >> CSK_ADCPDM_LATCH_DELAY_Pos) - 1;
         break;

    default:
        return CSK_ADCPDM_ERROR_LATCH_DELAY;
    }

    // PGA input mode (ADC ONLY, differential or single-ended)
    if ((control & CSK_ADCPDM_PGA_INPUT_Msk) == CSK_ADCPDM_PGA_INPUT_SET) {
        val = (arg >> 5) & 0x1; // bit[5] for Left(ADC0)
        reg_ctrl7.bit.AUD_EN_PGA0_SINGLE = val;
        reg_ctrl7.bit.AUD_EN_PGA0_VCMBUF = val; // VCMBUF set to 1 if Single-ended
        val = (arg >> 6) & 0x1; // bit[6] for Right(ADC1)
        reg_ctrl7.bit.AUD_EN_PGA1_SINGLE = val;
        reg_ctrl7.bit.AUD_EN_PGA1_VCMBUF = val; // VCMBUF set to 1 if Single-ended
        // set to register R12
        adc->reg->REG_AUD_ADC_CTRL7.all = reg_ctrl7.all;
    }

    // call apc_dual_channel_setup to notify APC channels
    // FIXME: should it support APC_CHMODE_24BITS_LOW?
    uint8_t ch_flag = (adc->info->use_16bits ? 1 : 0);
#if ADC_PDM_USE_PIO
    uint8_t ch_flag |= 0x2; //use PIO for TEST!
    LOGD("%s: used PIO to read RX FIFO!", __func__);
#endif
#if DAC_USE_GPDMA2D
    ch_flag |= 0x4;
#endif

    ret = apc_dual_channel_setup(adc->apc_dch,
                                APC_CHMODE_24BITS_HIGH,
                                adc->info->ch_bmp,
                                adc->info->ch_mix,
                                ch_flag);
    if (ret != CSK_DRIVER_OK)
        return ret;

    //------------------------------------------------
    // write settings into ADC/PDM registers
    //------------------------------------------------

    val = adc->info->flags & ADC_PDM_FLAG_CONFIGURED;

    if (val == 0) { // not configured, Control first called
        // enable CODEC external power, clock and pads if necessary, i.e. LDO etc.
        ENABLE_CODEC_BASIC();

        // CODEC analog settings
        if (!adc->info->use_pdm) //AMIC
            CONFIG_CODEC_ADC_ANALOG(adc->info->ch_bmp,
                    (reg_ctrl7.bit.AUD_EN_PGA1_SINGLE << 1) | reg_ctrl7.bit.AUD_EN_PGA0_SINGLE);

        reg_ctrl0.bit.LPGA_TOEN = 1; // Zero Crossing Time Out enable for ADC Left PGA gain
        reg_ctrl0.bit.RPGA_TOEN = 1; // Zero Crossing Time Out enable for ADC Right PGA gain
        reg_ctrl0.bit.ADC_CAP_CALI_GO = 0x1; //for cal

        //R6: VOL/Gain, HPF Setting etc.
        reg_ctrl1.all &= ~(ADCR_VOL_MASK | ADCL_VOL_MASK | ADCR_PGA_MASK | ADCL_PGA_MASK);
        reg_ctrl1.all |= ADCR_VOL(0x55) | ADCL_VOL(0x55) |   // L/R Digital Gain = 0dB
                    ADCR_PGA_VOL(6) | ADCL_PGA_VOL(6);    // L/R (Analog) PGA Vol = 0dB
                    // HPF2_EN | HPF1_EN | HPF_CUT(3);
        adc->info->d_vol_left = adc->info->d_vol_right = 0x55;

        //R12: reserve old settings about PGA Input mode & VCM Buffer
        reg_ctrl7.all &= RPGA_VCMBUF_EN | LPGA_VCMBUF_EN | RPGA_SINGLE | LPGA_SINGLE;
        reg_ctrl7.all |= RPGA_EN | RPGA_ZCEN | LPGA_EN | LPGA_ZCEN;

        //reg_ctrl4.all = adc->reg->REG_AUD_ADC_CTRL4.all;
        if (adc->info->use_pdm) { // DMIC
            reg_ctrl4.bit.AUTORST_TYPE = 0; //FIXME: 000b = 128us
            reg_ctrl4.bit.DMIC_SRC = 0; // FIXME: 0 = DMIC0
            reg_ctrl4.bit.DMIC_MODE = 1; // FIXME: 1= double edge on DMIC0 or DMIC1
            reg_ctrl4.bit.DMIC_LATCH_ADJ = 1;
            reg_ctrl4.bit.ALCHLD = 0; // FIXME: why use 0?
            reg_ctrl4.bit.ALCATK = 0xc; // FIXME: why use 0xc?
            reg_ctrl4.bit.ALCDCY = 6; // FIXME: why use 6?

            //reg_ctrl7.bit.AUD_EN_ADC0_VREF = 0; // VREF disabled
            //reg_ctrl7.bit.AUD_EN_ADC1_VREF = 0; // VREF disabled

            // remove ADC-specific settings except L/R channels' enable status
            reg_val = adc->reg->REG_AUD_ADC_CTRL6.all & ~ADCLR_EN;
            if (reg_val != 0)
                adc->reg->REG_AUD_ADC_CTRL6.all &= ADCLR_EN;

        } else { // AMIC
            reg_ctrl4.bit.AUTORST_TYPE = 1; // FIXME: 001 = 256us
            reg_ctrl4.bit.ALCHLD = 1; // FIXME: why use 1?
            reg_ctrl4.bit.ALCATK = 4; // FIXME: why use 4?
            reg_ctrl4.bit.ALCDCY = 6; // FIXME: why use 6?

            // set VCOM_SEL_VMID_75PER by default according to LuoSai??
            reg_ctrl7.all |= ADCL_VREF_EN | ADCR_VREF_EN; // VREF enabled // | PGA_VCOM_SEL_VMID_75PER

            //reg_ctrl7.bit.AUD_ADC_IDAC_OFFSET = 1; // feed back idac DC offset
            //reg_ctrl7.bit.RVAL_AUD_IDAC_BIAS = 1; // register value for aud_idac_bias
            //reg_ctrl7.bit.RSET_AUD_IDAC_BIAS = 1; // register control enable
            reg_ctrl7.all |= ADC_IDAC_OS_CTRL(IDAC_OS_20MV) | IDAC_BIAS_SRC | IDAC_BIAS_REG;

            // default 0x0000, set to 0x8000 (ADC analog clock invert)
            //adc->reg->REG_AUD_DEBUG_CFG.bit.REG_ADC_ANACLK_INV = 1;


            // R11: ADC-specific analog settings
            // bit[22]=1, rc calibration setting from register
            // bit[21:16]=0, quar calibration value from register.
            // bit[10]=1, 12MHz FS; bit[9]=1, bias current 10uA;
            // bit[8:7] = llb, Feedback IDAC Control +13%
            // bit[6:3] = 1000b, ADC amp ibias control
            //      0000: 1.5uA, 0001: 2uA, 0010: 2.5uA, 0011: 3uA
            //reg_val = 0x4003C0; // 0x4003C0; // 0x380; // 0x780;
            //
            // configure new settings except L/R channels' enable status
            reg_val = adc->reg->REG_AUD_ADC_CTRL6.all & ADCLR_EN;
//            reg_val |= ADC_RSET_RC_CALI | ADC_QUAR_COV(0x2f) |
//                      ADC_CBIAS_CUR_10UA | ADC_IDAC_7P5UA_13PERP | ADC_QUAR_COV_EN; // | ADC_IB_CTRL(8)
            reg_val |= ADC_CAP_CALI_EN_SRC_REG | ADC_CAP_CALI_EN_REG_ENA |
                       ADC_IDAC_CTRL(IDAC_7P5UA_P13P);

            //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
            if (adc->info->rc_ctrl != 0) {
                reg_val |= ADC_IB_CTRL(IB_CTRL_VAL_12MHZ);
                reg_ctrl7.all |= ADC_MODE_12MHZ;
            } else {
                reg_val |= ADC_IB_CTRL(IB_CTRL_VAL_DEF);
                reg_ctrl7.all &= ~ADC_MODE_12MHZ;
            }

            reg_val &= ~(ADC_LP_AUTORST_SPLIT | ADC_ANA_RST); // no reset for both L&R
            if (adc->reg->REG_AUD_ADC_CTRL6.all != reg_val)
                adc->reg->REG_AUD_ADC_CTRL6.all = reg_val;
        }
        adc->reg->REG_AUD_ADC_CTRL7.all = reg_ctrl7.all;
    }

    reg_ctrl0.bit.REG_ADC_RSTN = 1; // release (NOT RESET)
    adc->reg->REG_AUD_ADC_CTRL0.all = reg_ctrl0.all;
//    if (reg_ctrl0.all != adc->reg->REG_AUD_ADC_CTRL0.all)
//        adc->reg->REG_AUD_ADC_CTRL0.all = reg_ctrl0.all;

    if (reg_ctrl1.all != adc->reg->REG_AUD_ADC_CTRL1.all)
        adc->reg->REG_AUD_ADC_CTRL1.all = reg_ctrl1.all;

    if (reg_ctrl4.all != adc->reg->REG_AUD_ADC_CTRL4.all)
        adc->reg->REG_AUD_ADC_CTRL4.all = reg_ctrl4.all;

    if (val == 0) {
        enable_adc_pdm(adc, adc->info->ch_bmp); // ADC channel enable
        //adc->reg->REG_AUD_ADC_CTRL0.bit.REG_ADC_RSTN = 1;
    }

    // set configured flag if sampling rate is set...
    if ((control & CSK_ADCPDM_SR_Msk) != 0)
        adc->info->flags |= ADC_PDM_FLAG_CONFIGURED;

    return CSK_DRIVER_OK;
}

/**
 * @brief Set analog and/or digital volume of ADC/PDM device group
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param a_gain Analog gain (Left in low 16 bits, Right in high 16 bits)
 * @param d_gain Digital gain (Left in low 16 bits, Right in high 16 bits)
 * @param vol_flag Volume flag indicating which volumes to set
 * @return Execution status
 */
int32_t
ADC_PDM_SetVolume(void *adc_pdm_grp, uint32_t a_gain, uint32_t d_gain, uint32_t vol_flag)
{
    uint32_t val;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    if (adc == NULL) {
        CLOGW("%s: invalid ADC/PDM device group (0x%08x)!", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(vol_flag & ADC_PDM_BMP_STEREO) && !(vol_flag & (ADC_PDM_BMP_STEREO<<2))) {
        CLOGW("%s: invalid parameter, no volume is set: vol_flag=0x%08x",
                __func__, vol_flag);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        LOGD("%s: ADC/PDM device group %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    union AUD_ADC_CTRL1 reg_ctrl1;
    reg_ctrl1.all = adc->reg->REG_AUD_ADC_CTRL1.all;

    // Analog gain
    if (vol_flag & (ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_A_RIGHT)) {
        if (vol_flag & ADC_PDM_VOL_FLAG_A_LEFT) { // Left Analog
            val = a_gain & 0xFFFF;
            if (val > ADC_PGA_VOL_MAX || val < ADC_PGA_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_ctrl1.bit.ADC_PGA_LEVEL_L = val;
        }

        if (vol_flag & ADC_PDM_VOL_FLAG_A_RIGHT) { // Right Analog
            val = (a_gain >> 16) & 0xFFFF;
            if (val > ADC_PGA_VOL_MAX || val < ADC_PGA_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_ctrl1.bit.ADC_PGA_LEVEL_R = val;
        }
    }

    // Digital gain
    if (vol_flag & (ADC_PDM_VOL_FLAG_D_LEFT | ADC_PDM_VOL_FLAG_D_RIGHT)) {
        if (vol_flag & ADC_PDM_VOL_FLAG_D_LEFT) { // Left Digital
            val = d_gain & 0xFFFF;
            if (val > ADC_VOL_MAX || val < ADC_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_ctrl1.bit.ADCVOL_L = val;
            adc->info->d_vol_left = val;
        }

        if (vol_flag & ADC_PDM_VOL_FLAG_D_RIGHT) { // Right Digital
            val = (d_gain >> 16) & 0xFFFF;
            if (val > ADC_VOL_MAX || val < ADC_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_ctrl1.bit.ADCVOL_R = val;
            adc->info->d_vol_right = val;
        }
    }

    adc->reg->REG_AUD_ADC_CTRL1.all = reg_ctrl1.all;
    return CSK_DRIVER_OK;
}

/**
 * @brief Set mute/unmute value of ADC/PDM interface
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param mute_val Mute value (bit0 for Left, bit1 for Right)
 * @param dev_bmp Device bitmap
 * @return Execution status
 */
int32_t
ADC_PDM_SetMute(void *adc_pdm_grp, uint8_t mute_val, uint8_t dev_bmp)
{
    uint8_t mute;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp = 0;

    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device group: 0x%08x, dev_bmp: %d",
                __func__, adc_pdm_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        LOGD("%s: ADC/PDM device group %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    union AUD_ADC_CTRL1 reg_ctrl1;
    union AUD_ADC_CTRL7 reg_ctrl7;

    reg_ctrl1.all = adc->reg->REG_AUD_ADC_CTRL1.all;
    reg_ctrl7.all = adc->reg->REG_AUD_ADC_CTRL7.all;

    // Left channel mute setting
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        mute = mute_val & ADC_PDM_BMP_LEFT;
        if (mute) {
        	reg_ctrl1.bit.ADCVOL_L = 0; // digital mute
        	reg_ctrl7.bit.AUD_PGA0_MUTE = 1; // analog PGA mute
        } else {
        	reg_ctrl1.bit.ADCVOL_L = adc->info->d_vol_left; // restore original value
        	reg_ctrl7.bit.AUD_PGA0_MUTE = 0; // analog PGA unmute
        }
        adc->info->status.bit.l_mute = mute;
    }

    // Right channel mute setting
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        mute = (mute_val & ADC_PDM_BMP_RIGHT) >> 1;
        if (mute) {
        	reg_ctrl1.bit.ADCVOL_R = 0; // digital mute
        	reg_ctrl7.bit.AUD_PGA1_MUTE = 1; // analog PGA mute
        } else {
        	reg_ctrl1.bit.ADCVOL_R = adc->info->d_vol_right; // restore original value
            reg_ctrl7.bit.AUD_PGA1_MUTE = 0; // analog PGA unmute
        }
        adc->info->status.bit.r_mute = mute;
    }

    adc->reg->REG_AUD_ADC_CTRL1.all = reg_ctrl1.all;
    adc->reg->REG_AUD_ADC_CTRL7.all = reg_ctrl7.all;
    return CSK_DRIVER_OK;
}

/**
 * @brief Get ADC/PDM status
 * @param adc_pdm_grp Pointer to ADC/PDM device group instance
 * @param status Pointer to status structure
 * @return Execution status
 */
int32_t
ADC_PDM_GetStatus(void *adc_pdm_grp, CSK_ADCPDM_STATUS *status)
{
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);

    if (adc == NULL || status == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    (*status).all = adc->info->status.all;
    return CSK_DRIVER_OK;
}

/**
 * @brief APC event callback function
 * @param event_info Event information
 * @param usr_param User parameter (ADC/PDM group pointer)
 */
_FAST_FUNC_RO static void
adc_pdm_apc_event(uint32_t event_info, uint32_t usr_param)
{
    //uint8_t notify = 1;
    uint32_t adc_event_info;
    ADC_PDM_GRP *adc = (ADC_PDM_GRP *)usr_param;
    //uint8_t event_type = event_info & 0xFF;
    //APC_CH apc_ch = (event_info >> 8) & 0xFF;
    uint16_t event_type = APC_EVI_EVENT(event_info);
    APC_CH apc_ch = APC_EVI_CH(event_info);

    adc_event_info = (APC_CH_LR_IDX(apc_ch) << 8) | (APC_CH_TO_DCH(apc_ch) << 16);

    assert(adc != NULL);

    if (event_type & APC_EVENT_BLOCK_COMPLETE) {
//        if (event_info & PIPO_XFER_DONE) {
//            if (event_info & PIPO_PING_XFER_DONE)
//                adc_event_info |= CSK_ADCPDM_EVENT_RX_PING_DONE;
//            if (event_info & PIPO_PONG_XFER_DONE)
//                adc_event_info |= CSK_ADCPDM_EVENT_RX_PONG_DONE;
//        } else { // FIXME: SHOULD NOT COME HERE!
//            assert(0);
//            adc_event_info |= CSK_ADCPDM_EVENT_BLOCK_COMPLETE;
//        }
        adc_event_info |= CSK_ADCPDM_EVENT_BLOCK_COMPLETE;
    }

    if (event_type & APC_EVENT_TRANSFER_COMPLETE) {
        if (adc->info->ch_mix) // mixed L/R channels
            adc->info->status.bit.busy &= ~CH_BMP_STEREO;
        else if (APC_CH_LR_IDX(apc_ch)) // right channel
            adc->info->status.bit.busy &= ~CH_BMP_RIGHT;
        else // left channel
            adc->info->status.bit.busy &= ~CH_BMP_LEFT;
        adc_event_info |= CSK_ADCPDM_EVENT_RECEIVE_COMPLETE;
    }

    if (event_type & APC_EVENT_RX_FIFO_OVERRUN) {
        //adc->info->status.bit.busy = 0;
        adc->info->status.bit.rx_ovf = 1;
        adc_event_info |= CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN;
    }

    if (event_type & APC_EVENT_RX_FIFO_FULL) {
        adc->info->status.bit.rx_full = 1;
        adc_event_info |= CSK_ADCPDM_EVENT_RX_FIFO_FULL;
    }

    if (event_type & APC_EVENT_DMA_ERROR) {
        //adc->info->status.bit.busy = 0;
        adc_event_info |= CSK_ADCPDM_EVENT_OTHER_ERROR;
    }

    // notify ADC/PDM caller //notify &&
    if (adc->info->cb_event != NULL) {
        adc->info->cb_event(adc_event_info, adc->info->usr_param);
    }
}

//------------------------------------------------------------------------------------------

#if MIX_WITH_DAC_DIGTAL_ECHO
#include "../dac/dac.h"

/**
 * @brief Receive data from ADC and DAC echo channels (1ch ADC + 1ch ECHO, 2 channels total)
 * @param adc_pdm_grp Pointer to ADC device group
 * @param dac_grp Pointer to DAC device group
 * @param data Pointer to data buffer
 * @param num Number of samples to receive
 * @param dev_bmp Device bitmap (which channel to RX)
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_TwoReceive(void *adc_pdm_grp, void *dac_grp,
                    uint32_t *data, uint32_t num, uint8_t dev_bmp, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

    dev_bmp &= CH_BMP_STEREO;

    if (full_chk) {
    if (adc == NULL || dac == NULL || !(rx_flag & ADC_PDM_RX_FLAG_START_NOW) ||
        (dev_bmp != CH_BMP_LEFT && dev_bmp != CH_BMP_RIGHT)) {
        CLOGW("%s: invalid parameter, ADC/PDM & DAC devices: 0x%08x, 0x%08x",
                __func__, adc_pdm_grp, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
        !(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM or DAC has NOT been configured!!", __func__);
        return CSK_DRIVER_ERROR;
    }
    } // full check

    if (adc->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;


    ret = adc_disable_rx_channels(adc, dev_bmp);
    if (ret != CSK_DRIVER_OK)
        return ret;
    ret = dac_disable_echo_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (data != NULL) {
        TWCH_TYPE twch_type = (dev_bmp & CH_BMP_RIGHT) ? TWCH_ADC1_ECHO_DAC : TWCH_ADC0_ECHO_DAC;
        ret = apc_read_twi_channels (twch_type, data, num, full_chk);
    } else {
        ret = CSK_DRIVER_ERROR_PARAMETER;
    }
    if (ret != CSK_DRIVER_OK)
        return ret;

    adc->info->status.bit.busy |= dev_bmp;
    adc->info->status.bit.rx_full = 0;
    adc->info->status.bit.rx_ovf = 0;

    dac->info->status.bit.ech_busy |= CH_BMP_LEFT;
    dac->info->status.bit.ech_ovf = 0;

    ret = dac_enable_echo_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
    if(ret == CSK_DRIVER_OK) {
        //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
        ret = dac_enable_tx_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
        assert(ret == CSK_DRIVER_OK);
    }

    if(ret == CSK_DRIVER_OK) {
        ret = adc_enable_rx_channels(adc, dev_bmp);
        assert(ret == CSK_DRIVER_OK);
    }

    return ret;
}


/**
 * @brief Receive data from ADC and DAC echo channels (2ch ADC + 1ch ECHO, 3 channels)
 * @param adc_pdm_grp Pointer to ADC device group
 * @param dac_grp Pointer to DAC device group
 * @param data Pointer to data buffer
 * @param num Number of samples to receive
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_TriReceive(void *adc_pdm_grp, void *dac_grp,
                    uint32_t *data, uint32_t num, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

    if (full_chk) {
    if (adc == NULL || dac == NULL || !(rx_flag & ADC_PDM_RX_FLAG_START_NOW)) {
        CLOGW("%s: invalid parameter, ADC/PDM & DAC devices: 0x%08x, 0x%08x",
                __func__, adc_pdm_grp, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
        !(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM or DAC has NOT been configured!!", __func__);
        return CSK_DRIVER_ERROR;
    }
    } // full check

    if (adc->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    ret = adc_disable_rx_channels(adc, CH_BMP_STEREO);
    if (ret != CSK_DRIVER_OK)
        return ret;
    ret = dac_disable_echo_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (data != NULL)
        ret = apc_read_tri_channels (TCH_ADC01_ECHO_DAC, data, num, full_chk);
    else
        ret = CSK_DRIVER_ERROR_PARAMETER;
    if (ret != CSK_DRIVER_OK)
        return ret;

    adc->info->status.bit.busy |= CH_BMP_STEREO;
    adc->info->status.bit.rx_full = 0;
    adc->info->status.bit.rx_ovf = 0;

    dac->info->status.bit.ech_busy |= CH_BMP_LEFT;
    dac->info->status.bit.ech_ovf = 0;

    ret = dac_enable_echo_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
    if(ret == CSK_DRIVER_OK) {
        //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
        ret = dac_enable_tx_channels(dac, CH_BMP_LEFT); // CH_BMP_STEREO
        assert(ret == CSK_DRIVER_OK);
    }

    if(ret == CSK_DRIVER_OK) {
        ret = adc_enable_rx_channels(adc, CH_BMP_STEREO);
        assert(ret == CSK_DRIVER_OK);
    }

    return ret;
}


/**
 * @brief Receive data from ADC and DAC echo channels in Ping/Pong mode (1ch ADC + 1ch ECHO, 2 channels)
 * @param adc_pdm_grp Pointer to ADC device group
 * @param dac_grp Pointer to DAC device group
 * @param blks Pointer to PIPO blocks
 * @param blk_cnt_p Pointer to block count
 * @param dev_bmp Device bitmap (which channel to RX)
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_TwoReceive_PiPo(void *adc_pdm_grp, void *dac_grp, PIPO_IN_BLOCK *blks,
                    uint8_t *blk_cnt_p, uint8_t dev_bmp, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t bmp = 0;
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    dev_bmp &= CH_BMP_STEREO;
//    bmp = adc->info->ch_bmp;
    if (adc == NULL || dac == NULL || !(rx_flag & ADC_PDM_RX_FLAG_START_NOW) ||
        (dev_bmp != CH_BMP_LEFT && dev_bmp != CH_BMP_RIGHT)) {
        LOGD("%s: invalid parameter, ADC/PDM device: 0x%08x, DAC device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    bool started = (adc->info->status.bit.busy != 0);
    assert(started == (dac->info->status.bit.ech_busy != 0));

    if (!started) {
        if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
            !(dac->info->flags & DAC_FLAG_CONFIGURED)) {
            LOGD("%s: ADC/PDM or DAC group has NOT been configured!!",
                    __func__, adc->dev_idx);
            return CSK_DRIVER_ERROR;
        }
    } // !started

    TWCH_TYPE twch_type = (dev_bmp & CH_BMP_RIGHT) ? TWCH_ADC1_ECHO_DAC : TWCH_ADC0_ECHO_DAC;
    ret = apc_read_twi_channels_pipo (twch_type, blks, blk_cnt_p, full_chk);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        adc->info->status.bit.busy |= dev_bmp;
        adc->info->status.bit.rx_full = 0;
        adc->info->status.bit.rx_ovf = 0;

        dac->info->status.bit.ech_busy |= CH_BMP_LEFT;
        dac->info->status.bit.ech_ovf = 0;

        ret = dac_enable_echo_channels(dac, CH_BMP_LEFT);
        if (ret == CSK_DRIVER_OK) {
            ret = dac_enable_tx_channels(dac, CH_BMP_LEFT);
            //enable_dac(dac, dev_bmp); // enable DAC device group
        }

        if (ret == CSK_DRIVER_OK) {
            ret = adc_enable_rx_channels(adc, dev_bmp);
            //enable_adc_pdm(adc, dev_bmp); // enable ADC/PDM module
        }
    }

    return ret;
}


/**
 * @brief Receive data from ADC and DAC echo channels in Ping/Pong mode (2ch ADC + 1ch ECHO, 3 channels)
 * @param adc_pdm_grp Pointer to ADC device group
 * @param dac_grp Pointer to DAC device group
 * @param blks Pointer to PIPO blocks
 * @param blk_cnt_p Pointer to block count
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_TriReceive_PiPo(void *adc_pdm_grp, void *dac_grp, PIPO_IN_BLOCK *blks,
                    uint8_t *blk_cnt_p, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t bmp = 0;
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    bmp = adc->info->ch_bmp;
    if (adc == NULL || dac == NULL || bmp != ADC_PDM_BMP_STEREO || !(rx_flag & ADC_PDM_RX_FLAG_START_NOW)) {
        LOGD("%s: invalid parameter, ADC/PDM device: 0x%08x, DAC device: 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, dac_grp, bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    bool started = (adc->info->status.bit.busy != 0);
    assert(started == (dac->info->status.bit.ech_busy != 0));

    if (!started) {
        if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
            !(dac->info->flags & DAC_FLAG_CONFIGURED)) {
            LOGD("%s: ADC/PDM or DAC group has NOT been configured!!",
                    __func__, adc->dev_idx);
            return CSK_DRIVER_ERROR;
        }
    } // !started

    ret = apc_read_tri_channels_pipo (TCH_ADC01_ECHO_DAC, blks, blk_cnt_p, full_chk);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        adc->info->status.bit.busy |= bmp;
        adc->info->status.bit.rx_full = 0;
        adc->info->status.bit.rx_ovf = 0;

        dac->info->status.bit.ech_busy |= CH_BMP_LEFT;
        dac->info->status.bit.ech_ovf = 0;

        ret = dac_enable_echo_channels(dac, CH_BMP_LEFT);
        if (ret == CSK_DRIVER_OK) {
            ret = dac_enable_tx_channels(dac, CH_BMP_LEFT);
            //enable_dac(dac, bmp); // enable DAC device group
        }

        if (ret == CSK_DRIVER_OK) {
            ret = adc_enable_rx_channels(adc, bmp);
            //enable_adc_pdm(adc, bmp); // enable ADC/PDM module
        }
    }

    return ret;
}

#endif // MIX_WITH_DAC_DIGTAL_ECHO

#if MIX_WITH_I2S_DIGTAL_ECHO
#include "../i2s/i2s.h"
// (2ch ADC + 2ch ECHO)

/**
 * @brief Receive data from ADC and I2S echo channels (4 channels total)
 * @param adc_pdm_grp Pointer to ADC device group
 * @param i2s_out Pointer to I2S device
 * @param data Pointer to data buffer
 * @param num Number of samples to receive
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_QuadReceive(void *adc_pdm_grp, void *i2s_out,
                    uint32_t *data, uint32_t num, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    I2S_DEV *i2s = safe_i2s_dev(i2s_out);
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

    if (full_chk) {
    if (adc == NULL || i2s == NULL || i2s->dev_idx != 0) { // ONLY I2S0 OUT coupled with ADC01
        CLOGW("%s: invalid parameter, ADC/PDM & I2S (OUT) devices: 0x%08x, 0x%08x",
                __func__, adc_pdm_grp, i2s_out);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
        !(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM or I2S has NOT been configured!!", __func__);
        return CSK_DRIVER_ERROR;
    }
    } // full check

    if (adc->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t start_now = rx_flag & ADC_PDM_RX_FLAG_START_NOW;
    if (!start_now) {
        ret = adc_disable_rx_channels(adc, CH_BMP_STEREO);
        if (ret != CSK_DRIVER_OK)
            return ret;
        ret = i2s_disable_echo_channels(i2s, CH_BMP_STEREO);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    if (data != NULL)
        ret = apc_read_quad_channels (QCH_ADC01_ECHO_I2S1, data, num, full_chk);
    else
        ret = CSK_DRIVER_ERROR_PARAMETER;
    if (ret != CSK_DRIVER_OK)
        return ret;

    adc->info->status.bit.busy |= CH_BMP_STEREO;
    adc->info->status.bit.rx_full = 0;
    adc->info->status.bit.rx_ovf = 0;

    if (start_now) {
        ret = i2s_enable_echo_channels(i2s, CH_BMP_STEREO);
        if(ret == CSK_DRIVER_OK) {
            //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
            ret = i2s_enable_tx_channels(i2s, CH_BMP_STEREO);
            assert(ret == CSK_DRIVER_OK);
        }

        ret = adc_enable_rx_channels(adc, CH_BMP_STEREO);
        assert(ret == CSK_DRIVER_OK);
    }

    return ret;
}

/**
 * @brief Receive data from ADC and I2S echo channels in Ping/Pong mode
 * @param adc_pdm_grp Pointer to ADC device group
 * @param i2s_out Pointer to I2S device
 * @param blks Pointer to PIPO blocks
 * @param blk_cnt_p Pointer to block count
 * @param rx_flag Receive flags
 * @return Execution status
 */
int32_t
ADC_PDM_ECHO_QuadReceive_PiPo(void *adc_pdm_grp, void *i2s_out, PIPO_IN_BLOCK *blks,
                    uint8_t *blk_cnt_p, uint8_t rx_flag)
{
    int32_t ret = CSK_DRIVER_OK;
    uint8_t bmp = 0;
    bool full_chk = !(rx_flag & ADC_PDM_RX_FLAG_QUICK_CHK);

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    I2S_DEV *i2s = safe_i2s_dev(i2s_out);
    bmp = adc->info->ch_bmp;
    if (adc == NULL || i2s == NULL || bmp != ADC_PDM_BMP_STEREO || !(rx_flag & ADC_PDM_RX_FLAG_START_NOW)) {
        LOGD("%s: invalid parameter, ADC/PDM device: 0x%08x, I2S (OUT): 0x%08x, channel_bmp: %d",
                __func__, adc_pdm_grp, i2s_out, bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    bool started = (adc->info->status.bit.busy != 0);
    assert(started == (i2s->info->status.bit.ech_busy != 0));

    if (!started) {
        if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) ||
            !(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
            LOGD("%s: ADC/PDM group or I2S (OUT) has NOT been configured!!", __func__);
            return CSK_DRIVER_ERROR;
        }
    } // !started

    ret = apc_read_quad_channels_pipo (QCH_ADC01_ECHO_I2S1, blks, blk_cnt_p, full_chk);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        adc->info->status.bit.busy |= bmp;
        adc->info->status.bit.rx_full = 0;
        adc->info->status.bit.rx_ovf = 0;

        i2s->info->status.bit.ech_busy = 0x1;
        i2s->info->status.bit.ech_ovf = 0;

        ret = i2s_enable_echo_channels(i2s, CH_BMP_STEREO);
        if (ret == CSK_DRIVER_OK) {
            ret = i2s_enable_tx_channels(i2s, CH_BMP_STEREO);
            //enable_i2s(i2s); // enable I2S module
        }

        if (ret == CSK_DRIVER_OK) {
            ret = adc_enable_rx_channels(adc, bmp);
            //enable_adc_pdm(adc, bmp); // enable ADC/PDM module
        }
    }

    return ret;
}

#endif // MIX_WITH_I2S_DIGTAL_ECHO

#if (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)
// (2ch ADC + 1ch ECHO) OR (2ch ADC + 2ch ECHO)

/**
 * @brief Get transferred blocks count for Ping/Pong mode with echo
 * @param adc_pdm_grp Pointer to ADC device group
 * @param out_dev Pointer to output device (DAC or I2S)
 * @param blks Pointer to PIPO blocks
 * @param blk_cnt Block count
 * @param dev_bmp Device bitmap
 * @return Count of transferred blocks or error value
 */
int32_t
ADC_PDM_ECHO_PiPo_Xferred_Blocks(void *adc_pdm_grp, void *out_dev,
                PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp)
{
    int32_t ret;
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    uint8_t bmp, ech_dch;
    DAC_GRP *dac;
    I2S_DEV *i2s;

#if ADC_PDM_USE_PIO
    CLOGW("%s: PingPong is NOT supported for PIO!!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    // blks == NULL || blk_cnt == 0
    if (adc == NULL || (bmp = adc->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, ADC/PDM device: 0x%08x", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED) || !adc->info->status.bit.busy) {
        CLOGW("%s: ADC/PDM has NOT been configured or started!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    dac = safe_dac_grp(out_dev);
    if (dac != NULL) { // DAC ECHO
        if (!(dac->info->flags & DAC_FLAG_CONFIGURED) || !dac->info->status.bit.ech_busy) {
            CLOGW("%s: DAC has NOT been configured or started!!", __func__);
            return CSK_DRIVER_ERROR;
        }
        ech_dch = dac->dch_echo;

    } else { // NOT DAC ECHO
        i2s = safe_i2s_dev(out_dev);
        if (i2s == NULL)
            return CSK_DRIVER_ERROR_PARAMETER;
        if (!(i2s->info->flags & I2S_FLAG_CONFIGURED) || !i2s->info->status.bit.ech_busy) {
            CLOGW("%s: I2S (OUT) has NOT been configured or started!!", __func__);
            return CSK_DRIVER_ERROR;
        }
        ech_dch = i2s->apc_res.dch_echo;
    }

    // First get ECHO PingPong blocks
    ret = apc_dch_get_pipo_blks(ech_dch, CH_BMP_STEREO, (PIPO_IO_BLOCK *)blks, blk_cnt);
    if (ret < 0)
        return ret;

    assert(ret == 1);
    uint32_t xfer_cnt = blks->sample_cnt;

    // Then get ADC PingPong blocks
    // sum up xfer_cnt of ADC and ECHO channels, and use data buffer pointer for multi-channel
    ret = apc_dch_get_pipo_blks(adc->apc_dch, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
    if (ret < 0)
        return ret;

    assert(ret == 1);
    blks->sample_cnt += xfer_cnt;
    return ret;
}

#endif // (MIX_WITH_DAC_DIGTAL_ECHO || MIX_WITH_I2S_DIGTAL_ECHO)
