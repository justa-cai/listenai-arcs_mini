/*
 * adc_pdm.c
 *
 *
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "arcs_ap.h"
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

#define CSK_ADC_PDM_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

// driver version
static const
CSK_DRIVER_VERSION adc_pdm_driver_version = { CSK_ADC_PDM_API_VERSION, CSK_ADC_PDM_DRV_VERSION };

static void adc_pdm_apc_event(uint32_t event_info, uint32_t usr_param);

//------------------------------------------------------------------------------------------
static CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;
AUDIO_CODEC_RegDef *g_codec = (AUDIO_CODEC_RegDef *)AP_CODEC_BASE;

// ADC_PDM01
_FAST_DATA_VI static ADC_PDM_INFO adc01_info = { 0 };
_FAST_DATA_VI static ADC_PDM_GRP adc01_grp = {
        0, APC_DCH_ADC01,
        CSK_ADC_PDM_SAMPLE_BITS, 0,
        CSK_ADC01, // FIXME:
        &adc01_info
};

// Get Get ADC/PDM01 device group instance
//NOTE: ONLY 1 ADC (@Left channel) on ARCS
void* ADC_PDM01() { return &adc01_grp; }

//------------------------------------------------------------------------------------------
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

// enable ADC_PDM group (digital & analog) and its internal clock
static void enable_adc_pdm(ADC_PDM_GRP *adc, uint8_t dev_bmp)
{
    uint32_t reg_val, reg_org, val;
    assert(adc != NULL);

    if (adc->info->use_pdm) { // DMIC
        // R9: enable DMIC
        reg_val = adc->reg->REG_AUD_R9_ADC_CTRL4.all;
        if (!(reg_val & R9_DMIC_EN))
            adc->reg->REG_AUD_R9_ADC_CTRL4.all = reg_val | R9_DMIC_EN;

    } else { // AMIC
        // R9: disable DMIC
        reg_val = adc->reg->REG_AUD_R9_ADC_CTRL4.all;
        if (reg_val & R9_DMIC_EN)
            adc->reg->REG_AUD_R9_ADC_CTRL4.all = reg_val & ~R9_DMIC_EN;

        // R11: ADC-specific analog settings
        reg_org = reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all;

        val = dev_bmp & ADC_PDM_BMP_STEREO;
        if (val == ADC_PDM_BMP_STEREO)
            reg_val |= R11_ADCLR_EN; // enable ADC L/R
        else if (val == ADC_PDM_BMP_LEFT)
            reg_val |= R11_ADCL_EN; // enable ADC Left
        else if (val == ADC_PDM_BMP_RIGHT)
            reg_val |= R11_ADCR_EN; // enable ADC Right
        else {
            CLOGW("%s: invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
            return;
        }
        if (reg_val != reg_org)
            adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;

/*
        // it's a workaround to make sure RCCTRL(r5) is set to 12MHz after CALI_GO is done!
        if (adc->info->rc_ctrl != 0)
            adc->reg->REG_AUD_R12_ADC_CTRL7.bit.AUD_ADC_MODE = 1; //12MHz
            //adc->reg->REG_AUD_R12_ADC_CTRL7.all |= R12_ADC_MODE_12MHZ;
        else
            adc->reg->REG_AUD_R12_ADC_CTRL7.bit.AUD_ADC_MODE = 0; //4MHz
            //adc->reg->REG_AUD_R12_ADC_CTRL7.all &= ~R12_ADC_MODE_12MHZ;
*/
    }

    // R5: enable ADC internal clock
    reg_val = adc->reg->REG_AUD_R5_ADC_CTRL0.all;
    if (!(reg_val & R5_ADCCLK_EN)) {
        adc->reg->REG_AUD_R5_ADC_CTRL0.all = reg_val | R5_ADCCLK_EN;
    }
}

// disable ADC_PDM group (digital & analog) and its internal clock
static void disable_adc_pdm(ADC_PDM_GRP *adc) // for both L&R channels
{
    uint32_t reg_val;
    assert(adc != NULL);

    if (adc->info->use_pdm) { // DMIC
        // R9: disable DMIC
        adc->reg->REG_AUD_R9_ADC_CTRL4.all &= ~R9_DMIC_EN;
    } else { // AMIC
        // R11: disable ADC L/R
        adc->reg->REG_AUD_R11_ADC_CTRL6.all &= ~R11_ADCLR_EN;
    }

    reg_val = adc->reg->REG_AUD_R5_ADC_CTRL0.all;
    if (reg_val & R5_ADCCLK_EN) {
        // R5: disable ADC' all internal clocks
        adc->reg->REG_AUD_R5_ADC_CTRL0.all = reg_val & ~R5_ADCCLK_EN;
    }
}

// enable ADC_PDM group's external clock
static inline void enable_adc_pdm_clk(ADC_PDM_GRP *adc)
{
    //assert(adc != NULL);
#if (ARCS_VER < ARCS_D0_SOC) // B0, C0
    codec_clk_enable();
#else
    codec_adc_clk_enable(); // D0
#endif
}

static inline void disable_adc_pdm_clk(ADC_PDM_GRP *adc)
{
    //assert(adc != NULL);
#if (ARCS_VER < ARCS_D0_SOC) // B0, C0
    //codec_clk_disable();
#else
    codec_adc_clk_disable(); // D0
#endif
}

static inline void set_adc_low_power(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);
    uint32_t reg_val, reg_org;

    //R11 rc_ctrl & ib_ctrl
    reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all;
    //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
    //NOTE: DON'T change IB_CTRL when 12MHz currently...
    if (adc->info->rc_ctrl == 0) { // 4MHz
        reg_val &= ~R11_ADC_IB_CTRL_MASK;
        reg_val |= R11_ADC_IB_CTRL(IB_CTRL_VAL_LP);
        adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;
    }

    //R12 low power bits
    reg_org = reg_val = adc->reg->REG_AUD_R12_ADC_CTRL7.all;
    // INT1_LP & INT2_LP are default settings
    reg_val |= R12_PGA_VCOM_SEL_VMID_75PER | R12_ADC_SAR_COMP_LP | R12_PGA_LP |
            R12_ADC_INT2_LP | R12_ADC_INT1_LP;
    if (reg_val != reg_org)
        adc->reg->REG_AUD_R12_ADC_CTRL7.all = reg_val;
}

static inline void clear_adc_low_power(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);
    uint32_t reg_val, reg_org;

    //R12 low power bits
    reg_org = reg_val = adc->reg->REG_AUD_R12_ADC_CTRL7.all;
    reg_val &= ~(R12_PGA_VCOM_SEL_VMID_75PER | R12_PGA_LP);
    if (reg_val != reg_org)
        adc->reg->REG_AUD_R12_ADC_CTRL7.all = reg_val;

    //R11 rc_ctrl & ib_ctrl
    reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all;
    //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
    //NOTE: DON'T change IB_CTRL when 12MHz currently...
    if (adc->info->rc_ctrl == 0) { // 4MHz
        reg_val &= ~R11_ADC_IB_CTRL_MASK;
        reg_val |= R11_ADC_IB_CTRL(IB_CTRL_VAL_DEF);
        adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;
    }
}

//------------------------------------------------------------------------------------------

/**
 \fn          CSK_DRIVER_VERSION ADC_PDM_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
ADC_PDM_GetVersion()
{
    return adc_pdm_driver_version;
}


/**
 \fn          int32_t ADC_PDM_Initialize(void *adc_pdm_grp, ...)
 \brief       Initialize ADC/PDM device group interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   cb_event  Pointer to \ref CSK_ADC_PDM_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 (Left Channel, @bit[0]) and ADC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both ADC0 & ADC1
 \param[in]   use_flags  see below
 \return      \ref execution_status
*/
//int32_t
//ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
//                uint8_t dev_bmp, uint8_t use_flags)

int32_t
ADC_PDM_Initialize(void *adc_pdm_grp, CSK_ADC_PDM_SignalEvent_t cb_event, uint32_t usr_param,
                uint32_t dev_bmp_flag, ADC_PDM_DMA_CHS *dma_chs_p)
{
    g_sysctrl = IP_SYSCTRL;
    g_codec = (AUDIO_CODEC_RegDef *)AP_CODEC_BASE;

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
 \fn          int32_t ADC_PDM_Uninitialize(void *adc_pdm_grp)
 \brief       De-initialize ADC/PDM device group interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \return      \ref execution_status
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

    //TODO: any other clean operations?

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
 \fn          int32_t ADC_PDM_PowerControl(void *adc_pdm_grp, ...)
 \brief       Control ADC/PDM interface's Power.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
ADC_PDM_PowerControl(void *adc_pdm_grp, CSK_POWER_STATE state)
{
    ADC_PDM_GRP *adc = safe_adc_pdm_grp(adc_pdm_grp);
    if (adc == NULL) {
        CLOGW("%s: invalid ADC/PDM device (0x%08x)!", __func__, adc_pdm_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((adc->info->flags & ADC_PDM_FLAG_INITIALIZED) == 0)
        return CSK_DRIVER_ERROR;

    uint32_t val;
    switch (state) {
    case CSK_POWER_OFF:

        // abort current RX and clean state if configured
        if (adc->info->flags & ADC_PDM_FLAG_INITIALIZED)
            ADC_PDM_Abort(adc_pdm_grp, adc->info->ch_bmp);

        // disable ADC/PDM module
        disable_adc_pdm(adc);

        // disable ADC/PDM clock -- the clock is shared with other adc or i2s
        // FIXME: is it correct to disable the clock?
        disable_adc_pdm_clk(adc);

        //TODO: other power-off configuration...

        adc->info->flags &= ~(ADC_PDM_FLAG_POWERED | ADC_PDM_FLAG_CONFIGURED);
        break;

    case CSK_POWER_LOW:
        if ((adc->info->flags & ADC_PDM_FLAG_POWERED) != 0U) {
            if ((adc->info->flags & ADC_PDM_FLAG_LOW_POWER) == 0) {
                set_adc_low_power(adc);
                adc->info->flags |= ADC_PDM_FLAG_LOW_POWER;
            }
            return CSK_DRIVER_OK;
        }

        // enable ADC/PDM clock here
        enable_adc_pdm_clk(adc);

        //TODO: other power-on configuration...

        // ADC/PDM is powered & in low power state
        adc->info->flags |= ADC_PDM_FLAG_POWERED | ADC_PDM_FLAG_LOW_POWER;
        break;
        //return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:
        if ((adc->info->flags & ADC_PDM_FLAG_POWERED) != 0U) {
            if (adc->info->flags & ADC_PDM_FLAG_LOW_POWER) {
                clear_adc_low_power(adc);
                adc->info->flags &= ~ADC_PDM_FLAG_LOW_POWER;
            }
            return CSK_DRIVER_OK;
        }

        // remove disable operation, or else disable_enable operation will cause audio data deformed...
        //disable_adc_pdm(adc);

        // NOTE: enable ADC/PDM in advance when powered on, so as to avoid
        // taking in garbage recorded data during ADC/MIC initialization...
        //enable_adc_pdm(adc, adc->info->ch_bmp);

        // enable ADC/PDM clock here
        enable_adc_pdm_clk(adc);

        //TODO: other power-on configuration...

        // ADC/PDM is powered
        adc->info->flags |= ADC_PDM_FLAG_POWERED;
        break;

    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}


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
 \fn          int32_t ADC_PDM_Receive(void *adc_pdm_grp, ...)
 \brief       Receive data from ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[out]  data  Pointer to buffer to receive data from ADC/PDM interface
 \param[in]   num   Number of data items to receive
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 (Left Channel, @bit[0]) and ADC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both ADC0 & ADC1
 \param[in]   rx_flag   bit flags of receive operation
 \return      \ref execution_status
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


// Receive data via ADC/DMIC interface in the Ping/Pong mode
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


//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
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
 \fn          int32_t ADC_PDM_Receive_LLP(void *adc_pdm_grp, ...)
 \brief       Receive data from ADC/PDM interface into several non-continuous buffers.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   bufs  Pointer to list of buffers to receive data from ADC/PDM interface
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \param[in]   rx_flag   bit flags of receive operation
 \return      \ref execution_status
*/
/*
int32_t
ADC_PDM_Receive_LLP(void *adc_pdm_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t dev_bmp, uint8_t rx_flag)
{
    int32_t ret;
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
        ret = apc_dual_channel_read_LLP(adc->apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(adc->apc_dch, 0), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(adc->apc_dch, 1), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);

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
*/


/**
 \fn          int32_t ADC_PDM_Enable(void *adc_pdm_grp, ...)
 \brief       Enable ADC/PDM interface and data receiving is started from now on
              if ADC_PDM_Receive_XXX is called before.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
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
 \fn          int32_t ADC_PDM_Disable(void *adc_pdm_grp, ...)
 \brief       Disable ADC/PDM interface (and data receiving is suspended if any).
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
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
 \fn          int32_t ADC_PDM_Abort(void *adc_pdm_grp, ...)
 \brief       Abort ADC/PDM data transfer if any.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  ADC/PDM device bitmap (bit0 for ADC0 or ADC2, bit1 for ADC1 or ADC3)
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      \ref execution_status
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
 \fn          uint32_t ADC_PDM_GetRxCount(void *adc_pdm_grp, ...)
 \brief       Get received data count from ADC/PDM instance.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   dev_bmp  which ADC/PDM devices (similar to "I2S channels") are used,
              ADC0 or ADC2 (Left Channel, @bit[0]) and ADC1 or ADC3 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0(or ADC2) & DAC1(or ADC3)
 \return      number of data items (24-bit samples)transferred if positive, error value if negative.
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

    reg_val = adc->reg->REG_AUD_R5_ADC_CTRL0.bit.ADCSR;
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


static uint32_t adc_set_alc_params(ADC_PDM_GRP *adc, ALC_PARAMS *alc_p)
{
    union AUD_R8_ADC_CTRL3 reg_r8;
    union AUD_R9_ADC_CTRL4 reg_r9;
    uint8_t set_r9 = 0;
    assert(adc != NULL && alc_p != NULL);

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    reg_r8.all = adc->reg->REG_AUD_R8_ADC_CTRL3.all;
    reg_r9.all = adc->reg->REG_AUD_R9_ADC_CTRL4.all;

    if (alc_p->alc_flags & ALC_FLAG_ALC_SEL_L)
        reg_r8.bit.ALCSEL_L = alc_p->alc_sel_l;
    if (alc_p->alc_flags & ALC_FLAG_ALC_SEL_R)
        reg_r8.bit.ALCSEL_R = alc_p->alc_sel_r;
    if (alc_p->alc_flags & ALC_FLAG_ERR_TOLERANCE) {
        if (alc_p->err_tolerance > R8_ERR_TOLERANCE_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.TOLERANCE = alc_p->err_tolerance;
    }
    if (alc_p->alc_flags & ALC_FLAG_TARGET_L) {
        if (alc_p->target_l > R8_TARGET_LEVEL_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.TARGET_L = alc_p->target_l;
    }
    if (alc_p->alc_flags & ALC_FLAG_TARGET_R) {
        if (alc_p->target_r > R8_TARGET_LEVEL_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.TARGET_R = alc_p->target_r;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_MODE)
        reg_r8.bit.ALCMODE = alc_p->alc_mode;
    if (alc_p->alc_flags & ALC_FLAG_NGATE_EN)
        reg_r8.bit.NG_EN = alc_p->ngate_en;
    if (alc_p->alc_flags & ALC_FLAG_NGATE_FLOOR) {
        if (alc_p->ngate_floor > R8_NGATE_FLOOR_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.NG = alc_p->ngate_floor;
    }

    if (alc_p->alc_flags & ALC_FLAG_ALC_MIN) {
        if (alc_p->alc_min > R8_ALCMIN_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.ALCMIN = alc_p->alc_min;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_MAX) {
        if (alc_p->alc_max > R8_ALCMAX_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r8.bit.ALCMAX = alc_p->alc_max;
    }
    if (alc_p->alc_flags & (ALC_FLAG_ALC_MIN | ALC_FLAG_ALC_MAX)) {
        if (reg_r8.bit.ALCMIN > reg_r8.bit.ALCMAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
    }

    if (alc_p->alc_flags & ALC_FLAG_ALC_HOLD) {
        if (alc_p->alc_hold > R9_ALC_HOLD_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r9.bit.ALCHLD = alc_p->alc_hold;
        set_r9 = 1;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_ATTACK) {
        if (alc_p->alc_attack > R9_ALC_ATTACK_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r9.bit.ALCATK = alc_p->alc_attack;
        set_r9 = 1;
    }
    if (alc_p->alc_flags & ALC_FLAG_ALC_DECAY) {
        if (alc_p->alc_decay > R9_ALC_DECAY_MAX)
            return CSK_ADCPDM_ERROR_ALC_PARAMS;
        reg_r9.bit.ALCDCY = alc_p->alc_decay;
        set_r9 = 1;
    }

    adc->reg->REG_AUD_R8_ADC_CTRL3.all = reg_r8.all;
    if (set_r9)
        adc->reg->REG_AUD_R9_ADC_CTRL4.all = reg_r9.all;

    return CSK_DRIVER_OK;
}


static uint32_t adc_do_cali(ADC_PDM_GRP *adc)
{
    assert(adc != NULL);

    if (!(adc->info->flags & ADC_PDM_FLAG_CONFIGURED)) {
        CLOGW("%s: ADC/PDM couple %d has NOT been configured!!",
                __func__, adc->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    // clear and then set ADC_CAP_CALI_GO bit
    adc->reg->REG_AUD_R5_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 0x0; // clear it
    adc->reg->REG_AUD_R5_ADC_CTRL0.bit.ADC_CAP_CALI_GO = 0x1; // set it to start

    // it takes dozens of us to complete the calibration process...
    volatile uint32_t count = 300 * 10; //FIXME: 300 * 40 or bigger?
    while (count-- > 0);

    return CSK_DRIVER_OK;
}


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

                // R11: ADC-specific analog settings
                uint32_t reg_val;
                reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all;
                reg_val &= ~R11_ADC_IB_CTRL_MASK;

                //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
                //if (adc->info->rc_ctrl != 0 && !(adc->info->flags & ADC_PDM_FLAG_LOW_POWER))
                if (adc->info->rc_ctrl != 0) {
                    reg_val |=R11_ADC_IB_CTRL(IB_CTRL_VAL_12MHZ); //12MHz, 2.5uA
                    adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;
                    adc->reg->REG_AUD_R12_ADC_CTRL7.all |= R12_ADC_MODE_12MHZ; // 12MHz
                } else {
                    reg_val |= R11_ADC_IB_CTRL(IB_CTRL_VAL_DEF); //2.0uA
                    adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;
                    adc->reg->REG_AUD_R12_ADC_CTRL7.all &= ~R12_ADC_MODE_12MHZ; // 4MHz
                }

                return true;
            }
        } // end for

    }
    return false;
}


/**
 \fn          int32_t ADC_PDM_Control(void *adc_pdm_grp, ...)
 \brief       Control ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   control  Operation
 \param[in]   arg  Argument of operation (optional), i.e. sample rate
 \return      common \ref execution_status and driver specific \ref ADC_PDM execution_status
*/
int32_t
ADC_PDM_Control(void *adc_pdm_grp, uint32_t control, uint32_t arg)
{
    int32_t ret;
    uint32_t val, reg_val; //, mclk_flag
    union AUD_R5_ADC_CTRL0 reg_r5;
    union AUD_R6_ADC_CTRL1 reg_r6;
    union AUD_R9_ADC_CTRL4 reg_r9;
    union AUD_R12_ADC_CTRL7 reg_r12;

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
        reg_r6.all = adc->reg->REG_AUD_R6_ADC_CTRL1.all;
        reg_r6.bit.HPF1EN = arg & 0x1; // bit[0]
        reg_r6.bit.HPF2EN = (arg >> 1) & 0x1; // bit[1]
        reg_r6.bit.HPFCUT = (arg >> 2) & 0x7; // bit[4:2]
        adc->reg->REG_AUD_R6_ADC_CTRL1.all = reg_r6.all;
        // remove HPF setting bits after done and just return if no other setting
        control &= ~CSK_ADCPDM_HPF_Msk;
        if (control == 0)
            return CSK_DRIVER_OK;
    }

    if (adc->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    reg_r5.all = adc->reg->REG_AUD_R5_ADC_CTRL0.all;
    reg_r6.all = adc->reg->REG_AUD_R6_ADC_CTRL1.all;
    reg_r9.all = adc->reg->REG_AUD_R9_ADC_CTRL4.all;
    reg_r12.all = adc->reg->REG_AUD_R12_ADC_CTRL7.all;

//    // 24MHz main clock by default
//    reg_r5.bit.AUD_ADC_MCLK_SEL = 0;
//    // How to set main clock? mclk_flag options:
//    // 0 = use 24MHz, 1 = use 22.05MHz, 2 = not changed
//    // NOTE: AON_CODEC(ADC01) supports 24MHz main clock only!!
//    mclk_flag = 0;

    // Sample Rate
    switch (control & CSK_ADCPDM_SR_Msk) {
    // Keep Sample Rate unchanged
    case CSK_ADCPDM_SR_UNSET:
//        mclk_flag = 2; // not changed
        break;

    case CSK_ADCPDM_SR_8KHZ:
        reg_r5.bit.ADCSR = 0x0;
        adc->info->samp_freq = 8000;
        break;

    case CSK_ADCPDM_SR_16KHZ:
        reg_r5.bit.ADCSR = 0x3;
        adc->info->samp_freq = 16000;
        break;

//    case CSK_ADCPDM_SR_44P1KHZ:
//        if (adc->dev_idx == 0) // ADC01 support 24MHz only
//            return CSK_ADCPDM_ERROR_SAMP_RATE;
//        mclk_flag = 1; // use 22.05MHz
//        reg_r5.bit.AUD_ADC_MCLK_SEL = 1; // 22.05MHz main clock
//        reg_r5.bit.ADCSR = 0x7;
//        adc->info->samp_freq = 44100;
//        break;

    case CSK_ADCPDM_SR_48KHZ:
        reg_r5.bit.ADCSR = 0x8;
        adc->info->samp_freq = 48000;
        break;

    default:
        return CSK_ADCPDM_ERROR_SAMP_RATE;
    }

//    if (mclk_flag == 0) { // use 24Mhz
//        adc_clk_select(AUDIO_CLK_SRC_XTAL);
//    } else if (mclk_flag == 1) { // use 22.05Mhz
//        uint8_t div_n = BOARD_BOOTCLOCKRUN_AUDPLL_DIV_N;
//        uint8_t div_m = BOARD_BOOTCLOCKRUN_AUDPLL_DIV_M;
//        AUDIOPLL_InitPLL(div_n, div_m, BOARD_BOOTCLOCKRUN_AUDPLL_DIV_NF);
//        init_audpll_audio_clk(0); // DEF_AUDPLL_AUDIO_FREQ (22050000)
//        adc_clk_select(AUDIO_CLK_SRC_AUDPLL);
//    }

    // Over Sample Ratio
    switch (control & CSK_ADCPDM_OSR_Msk) {
    // Keep Over Sample Ratio unchanged
    case CSK_ADCPDM_OSR_UNSET:
        break;

    case CSK_ADCPDM_OSR_500:
        reg_r5.bit.ADCOSR = 0x0;
        adc->info->over_samp_ratio = 500;
        break;

    case CSK_ADCPDM_OSR_250:
        reg_r5.bit.ADCOSR = 0x1;
        adc->info->over_samp_ratio = 250;
        break;

    case CSK_ADCPDM_OSR_125:
        reg_r5.bit.ADCOSR = 0x2;
        adc->info->over_samp_ratio = 125;
        break;

    case CSK_ADCPDM_OSR_100:
        reg_r5.bit.ADCOSR = 0x3;
        adc->info->over_samp_ratio = 100;
        break;

    case CSK_ADCPDM_OSR_50:
        reg_r5.bit.ADCOSR = 0x4;
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
        reg_r9.bit.DMIC_LATCH_ADJ = (val >> CSK_ADCPDM_LATCH_DELAY_Pos) - 1;
         break;

    default:
        return CSK_ADCPDM_ERROR_LATCH_DELAY;
    }

    // PGA input mode (ADC ONLY, differential or single-ended)
    if ((control & CSK_ADCPDM_PGA_INPUT_Msk) == CSK_ADCPDM_PGA_INPUT_SET) {
        val = (arg >> 5) & 0x1; // bit[5] for Left(ADC0)
        reg_r12.bit.AUD_EN_PGA0_SINGLE = val;
        reg_r12.bit.AUD_EN_PGA0_VCMBUF = val; // VCMBUF set to 1 if Single-ended
        val = (arg >> 6) & 0x1; // bit[6] for Right(ADC1)
        reg_r12.bit.AUD_EN_PGA1_SINGLE = val;
        reg_r12.bit.AUD_EN_PGA1_VCMBUF = val; // VCMBUF set to 1 if Single-ended
        // set to register R12
        adc->reg->REG_AUD_R12_ADC_CTRL7.all = reg_r12.all;
    }

    // call apc_dual_channel_setup to notify APC channels
    // FIXME: should it support APC_CHMODE_24BITS_LOW?
#if ADC_PDM_USE_PIO
    uint8_t ch_flag = (adc->info->use_16bits ? 1 : 0) | 0x2; //use PIO for TEST!
    LOGD("%s: used PIO to read RX FIFO!", __func__);
#else
    uint8_t ch_flag = (adc->info->use_16bits ? 1 : 0);
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
                    (reg_r12.bit.AUD_EN_PGA1_SINGLE << 1) | reg_r12.bit.AUD_EN_PGA0_SINGLE);

        reg_r5.bit.LPGA_TOEN = 1; // Zero Crossing Time Out enable for ADC Left PGA gain
        reg_r5.bit.RPGA_TOEN = 1; // Zero Crossing Time Out enable for ADC Right PGA gain
        reg_r5.bit.ADC_CAP_CALI_GO = 0x1; //for cal

        //R6: VOL/Gain, HPF Setting etc.
        reg_r6.all &= ~(R6_ADCR_VOL_MASK | R6_ADCL_VOL_MASK | R6_ADCR_PGA_MASK | R6_ADCL_PGA_MASK);
        reg_r6.all |= R6_ADCR_VOL(0x55) | R6_ADCL_VOL(0x55) |   // L/R Digital Gain = 0dB
                    R6_ADCR_PGA_VOL(6) | R6_ADCL_PGA_VOL(6);    // L/R (Analog) PGA Vol = 0dB
                    // R6_HPF2_EN | R6_HPF1_EN | R6_HPF_CUT(3);
        adc->info->d_vol_left = adc->info->d_vol_right = 0x55;

        //R12: reserve old settings about PGA Input mode & VCM Buffer
        reg_r12.all &= R12_RPGA_VCMBUF_EN | R12_LPGA_VCMBUF_EN | R12_RPGA_SINGLE | R12_LPGA_SINGLE;
        reg_r12.all |= R12_RPGA_EN | R12_RPGA_ZCEN | R12_LPGA_EN | R12_LPGA_ZCEN;

        //reg_r9.all = adc->reg->REG_AUD_R9_ADC_CTRL4.all;
        if (adc->info->use_pdm) { // DMIC
            // R9:
            reg_r9.bit.AUTORST_TYPE = 0; //FIXME: 000b = 128us
            reg_r9.bit.DMIC_SRC = 0; // FIXME: 0 = DMIC0
            reg_r9.bit.DMIC_MODE = 1; // FIXME: 1= double edge on DMIC0 or DMIC1
            reg_r9.bit.DMIC_LATCH_ADJ = 1;
            reg_r9.bit.ALCHLD = 0; // FIXME: why use 0?
            reg_r9.bit.ALCATK = 0xc; // FIXME: why use 0xc?
            reg_r9.bit.ALCDCY = 6; // FIXME: why use 6?

            // R12:
            //reg_r12.bit.AUD_EN_ADC0_VREF = 0; // VREF disabled
            //reg_r12.bit.AUD_EN_ADC1_VREF = 0; // VREF disabled

            // R11: remove ADC-specific settings except L/R channels' enable status
            reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all & ~R11_ADCLR_EN;
            if (reg_val != 0)
                adc->reg->REG_AUD_R11_ADC_CTRL6.all &= R11_ADCLR_EN;

        } else { // AMIC
            // R9:
            reg_r9.bit.AUTORST_TYPE = 1; // FIXME: 001 = 256us
            reg_r9.bit.ALCHLD = 1; // FIXME: why use 1?
            reg_r9.bit.ALCATK = 4; // FIXME: why use 4?
            reg_r9.bit.ALCDCY = 6; // FIXME: why use 6?

            // R12: set VCOM_SEL_VMID_75PER by default according to LuoSai??
            reg_r12.all |= R12_ADCL_VREF_EN | R12_ADCR_VREF_EN; // VREF enabled // | R12_PGA_VCOM_SEL_VMID_75PER

            //reg_r12.bit.AUD_ADC_IDAC_OFFSET = 1; // feed back idac DC offset
            //reg_r12.bit.RVAL_AUD_IDAC_BIAS = 1; // register value for aud_idac_bias
            //reg_r12.bit.RSET_AUD_IDAC_BIAS = 1; // register control enable
            reg_r12.all |= R12_ADC_IDAC_OS_CTRL(IDAC_OS_20MV) | R12_IDAC_BIAS_SRC | R12_IDAC_BIAS_REG;

            // R1: default 0x0000, set to 0x8000 (ADC analog clock invert)
            //adc->reg->REG_AUD_R1_GLOBAL0.bit.ADC_ANACLK_INV = 1;

//            //FIXME: R0: 0xf001 on AON_CODEC, 0xf002 on CP_CODEC?
//            if (adc->dev_idx == 0) // AON_CODEC
//                adc->reg->REG_AUD_R0_RSVD_REG0.bit.ADC01_DESKEW_CLK_INV = 1;
//            if (adc->dev_idx == 1) // CP_CODEC
//                adc->reg->REG_AUD_R0_RSVD_REG0.bit.ADC23_DESKEW_CLK_INV = 1;

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
            reg_val = adc->reg->REG_AUD_R11_ADC_CTRL6.all & R11_ADCLR_EN;
//            reg_val |= R11_ADC_RSET_RC_CALI | R11_ADC_QUAR_COV(0x2f) |
//                      R11_ADC_CBIAS_CUR_10UA | R11_ADC_IDAC_7P5UA_13PERP | R11_ADC_QUAR_COV_EN; // | R11_ADC_IB_CTRL(8)
            reg_val |= R11_ADC_CAP_CALI_EN_SRC_REG | R11_ADC_CAP_CALI_EN_REG_ENA | //FIXME: CALI setting OK?
                       R11_ADC_IDAC_CTRL(IDAC_7P5UA_P13P);

            //NOTE: DON'T change ADC 12M or 4MHz mode whether low power or not!
            if (adc->info->rc_ctrl != 0) {
                reg_val |= R11_ADC_IB_CTRL(IB_CTRL_VAL_12MHZ);
                reg_r12.all |= R12_ADC_MODE_12MHZ;
            } else {
                reg_val |= R11_ADC_IB_CTRL(IB_CTRL_VAL_DEF);
                reg_r12.all &= ~R12_ADC_MODE_12MHZ;
            }

            reg_val &= ~(R11_ADC_LP_AUTORST_SPLIT | R11_ADC_ANA_RST); // no reset for both L&R
            if (adc->reg->REG_AUD_R11_ADC_CTRL6.all != reg_val)
                adc->reg->REG_AUD_R11_ADC_CTRL6.all = reg_val;
        }
        adc->reg->REG_AUD_R12_ADC_CTRL7.all = reg_r12.all;

    #if (ARCS_VER <= ARCS_C0_SOC)
        // set FILGAIN from register to fix digital gain adjust step issue according to ZhaoRui
        reg_val = adc->reg->REG_AUD_R10_ADC_CTRL5.all;
        reg_val &= ~R10_FILGAIN_REG_MASK;
        reg_val |= R10_FILGAIN_REG(0x10000) | R10_FILGAIN_REGEN;
        adc->reg->REG_AUD_R10_ADC_CTRL5.all = reg_val;
    #endif
    }

//    // R5
    reg_r5.bit.REG_ADC_RSTN = 1; // release (NOT RESET)
    adc->reg->REG_AUD_R5_ADC_CTRL0.all = reg_r5.all;
//    if (reg_r5.all != adc->reg->REG_AUD_R5_ADC_CTRL0.all)
//        adc->reg->REG_AUD_R5_ADC_CTRL0.all = reg_r5.all;

    // R6
    if (reg_r6.all != adc->reg->REG_AUD_R6_ADC_CTRL1.all)
        adc->reg->REG_AUD_R6_ADC_CTRL1.all = reg_r6.all;
    // R9
    if (reg_r9.all != adc->reg->REG_AUD_R9_ADC_CTRL4.all)
        adc->reg->REG_AUD_R9_ADC_CTRL4.all = reg_r9.all;

    if (val == 0) {
        enable_adc_pdm(adc, adc->info->ch_bmp); //R11 ADC channel enable
        //adc->reg->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;
    }

    // set configured flag if sampling rate is set...
    if ((control & CSK_ADCPDM_SR_Msk) != 0)
        adc->info->flags |= ADC_PDM_FLAG_CONFIGURED;

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t ADC_PDM_SetVolume(void *adc_pdm_grp, ...)
 \brief       Set analog and/or digital of ADC/PDM device group.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   a_gain    Analog Gain of Left Channel (ADC0, @ a_gain[15:0])
                        and Right Channel (ADC1, @ a_gain[31:16]), for ADC only (NOT for PDM/DMIC)
                        Analog Gain value: 0x0 (-12dB) ~ 0x18 (+36dB), 2dB each step, default 0x6 (0dB)
 \param[in]   d_gain    Digital gain of Left Channel (ADC/PDM0, @ d_gain[15:0])
                        and Right Channel (ADC/PDM1, @ d_gain[31:16]),
                        Digital Gain value: 0x0 (-83dB) ~ 0x7F (+42dB), 1dB each step, default 0x55 (0dB)
 \param[in]   vol_flag  volume flag which indicates which volume items are specified
 \return      common \ref execution_status and driver specific \ref ADC/PDM execution_status
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

    union AUD_R6_ADC_CTRL1 reg_r6;
    reg_r6.all = adc->reg->REG_AUD_R6_ADC_CTRL1.all;

    // Analog gain
    if (vol_flag & (ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_A_RIGHT)) {
        if (vol_flag & ADC_PDM_VOL_FLAG_A_LEFT) { // Left Analog
            val = a_gain & 0xFFFF;
            if (val > R6_ADC_PGA_VOL_MAX || val < R6_ADC_PGA_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r6.bit.ADC_PGA_LEVEL_L = val;
        }

        if (vol_flag & ADC_PDM_VOL_FLAG_A_RIGHT) { // Right Analog
            val = (a_gain >> 16) & 0xFFFF;
            if (val > R6_ADC_PGA_VOL_MAX || val < R6_ADC_PGA_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r6.bit.ADC_PGA_LEVEL_R = val;
        }
    }

    // Digital gain
    if (vol_flag & (ADC_PDM_VOL_FLAG_D_LEFT | ADC_PDM_VOL_FLAG_D_RIGHT)) {
        if (vol_flag & ADC_PDM_VOL_FLAG_D_LEFT) { // Left Digital
            val = d_gain & 0xFFFF;
            if (val > R6_ADC_VOL_MAX || val < R6_ADC_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r6.bit.ADCVOL_L = val;
            adc->info->d_vol_left = val;
        }

        if (vol_flag & ADC_PDM_VOL_FLAG_D_RIGHT) { // Right Digital
            val = (d_gain >> 16) & 0xFFFF;
            if (val > R6_ADC_VOL_MAX || val < R6_ADC_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r6.bit.ADCVOL_R = val;
            adc->info->d_vol_right = val;
        }
    }

    adc->reg->REG_AUD_R6_ADC_CTRL1.all = reg_r6.all;
    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t ADC_PDM_SetMute(void *adc_pdm_grp, ...)
 \brief       Set mute/unmute value of ADC/PDM interface.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[in]   mute_val  Mute/UnMute bit of Left Channel (ADC/PDM0, @ mute_val[0])
                        and Right Channel (ADC/PDM1, @ mute_val[1]),
 \param[in]   dev_bmp  specify which ADC/PDM devices in the device group
                      ADC/PDM0 (Left Channel, @bit[0]) and ADC/PDM1 (Right Channel, @bit[1]),
                      and dev_bmp = 0x3 indicates both ADC/PDM0 & ADC/PDM1
 \return      common \ref execution_status and driver specific \ref ADC/PDM execution_status
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

    union AUD_R6_ADC_CTRL1 reg_r6;
    union AUD_R12_ADC_CTRL7 reg_r12;

    reg_r6.all = adc->reg->REG_AUD_R6_ADC_CTRL1.all;
    reg_r12.all = adc->reg->REG_AUD_R12_ADC_CTRL7.all;

    // Left channel mute setting
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        mute = mute_val & ADC_PDM_BMP_LEFT;
        if (mute) {
            reg_r6.bit.ADCVOL_L = 0; // digital mute
            reg_r12.bit.AUD_PGA0_MUTE = 1; // analog PGA mute
        } else {
            reg_r6.bit.ADCVOL_L = adc->info->d_vol_left; // restore original value
            reg_r12.bit.AUD_PGA0_MUTE = 0; // analog PGA unmute
        }
        adc->info->status.bit.l_mute = mute;
    }

    // Right channel mute setting
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        mute = (mute_val & ADC_PDM_BMP_RIGHT) >> 1;
        if (mute) {
            reg_r6.bit.ADCVOL_R = 0; // digital mute
            reg_r12.bit.AUD_PGA1_MUTE = 1; // analog PGA mute
        } else {
            reg_r6.bit.ADCVOL_R = adc->info->d_vol_right; // restore original value
            reg_r12.bit.AUD_PGA1_MUTE = 0; // analog PGA unmute
        }
        adc->info->status.bit.r_mute = mute;
    }

    adc->reg->REG_AUD_R6_ADC_CTRL1.all = reg_r6.all;
    adc->reg->REG_AUD_R12_ADC_CTRL7.all = reg_r12.all;
    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t ADC_PDM_GetStatus(void *adc_pdm_grp, ...)
 \brief       Get ADC/PDM status.
 \param[in]   adc_pdm_grp  Pointer to ADC/PDM device group instance
 \param[out]  status  Pointer to CSK_ADC_PDM_STATUS buffer
 \return      \ref execution_status
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


_FAST_FUNC_RO static void
adc_pdm_apc_event(uint32_t event_info, uint32_t usr_param)
{
    //uint8_t notify = 1;
    uint32_t adc_event_info;
    ADC_PDM_GRP *adc = (ADC_PDM_GRP *)usr_param;
    uint8_t event_type = event_info & 0xFF;

    APC_CH apc_ch = (event_info >> 8) & 0xFF;
    adc_event_info = (APC_CH_LR_IDX(apc_ch) << 8) | (APC_CH_TO_DCH(apc_ch) << 16);

    assert(adc != NULL);

    if (event_type & APC_EVENT_BLOCK_COMPLETE) {
        if (event_info & PIPO_XFER_DONE) {
            if (event_info & PIPO_PING_XFER_DONE)
                adc_event_info |= CSK_ADCPDM_EVENT_RX_PING_DONE;
            if (event_info & PIPO_PONG_XFER_DONE)
                adc_event_info |= CSK_ADCPDM_EVENT_RX_PONG_DONE;
        } else { // FIXME: SHOULD NOT COME HERE!
            assert(0);
            adc_event_info |= CSK_ADCPDM_EVENT_BLOCK_COMPLETE;
        }
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
// (2ch ADC + 1ch ECHO)

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


// Receive data via ADC/DMIC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
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
        ret = apc_read_quad_channels (QCH_ADC01_ECHO_I2S0, data, num, full_chk);
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


// Receive data via ADC/DMIC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
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

    ret = apc_read_quad_channels_pipo (QCH_ADC01_ECHO_I2S0, blks, blk_cnt_p, full_chk);
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

//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
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
