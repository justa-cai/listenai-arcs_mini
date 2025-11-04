/*
 * dac.c
 *
 *
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "arcs_ap.h"
#include "Driver_DAC.h"
#include "dac.h"
#include "ClockManager.h" // for CRM_GetSrcFreq()

#include "log_print.h"

#define DEBUG_LOG   0 //1 //
#if DEBUG_LOG
#define LOGD(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define DAC_USE_PIO     0 //1 //RAM -> TX FIFO, 1: use PIO, 0: use DMA (default)

#define CSK_DAC_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)

// driver version
static const
CSK_DRIVER_VERSION dac_driver_version = { CSK_DAC_API_VERSION, CSK_DAC_DRV_VERSION };

static void dac_apc_event(uint32_t event_info, uint32_t usr_param);

//------------------------------------------------------------------------------------------
static CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;
static AUDIO_CODEC_RegDef *g_codec = (AUDIO_CODEC_RegDef *)AP_CODEC_BASE;

// DAC01
_FAST_DATA_VI static DAC_INFO dac01_info = { 0 };
_FAST_DATA_VI static DAC_GRP dac01_grp = {
        0, APC_DCH_DAC01, APC_DCH_ECHO0, //FIXME: check echo dual_channels!
        CSK_DAC_SAMPLE_BITS,
        CSK_DAC01,
        &dac01_info
};

// Get Get DAC01 device instance
void* DAC01() { return &dac01_grp; }

//------------------------------------------------------------------------------------------

DAC_GRP * safe_dac_grp(void *dac_grp)
{
    // safe check of ADC/PDM device parameter
    if (dac_grp == &dac01_grp) {
        if (dac01_grp.reg != CSK_DAC01 || dac01_grp.apc_dch != APC_DCH_DAC01 || dac01_grp.dev_idx != 0) {
            CLOGW("DAC device context has been tampered illegally!!\n");
            return NULL;
        }
    } else {
        return NULL;
    }

    return (DAC_GRP *)dac_grp;
}

// enable DAC group (digital & analog) and its internal clock
static void enable_dac(DAC_GRP *dac, uint8_t dev_bmp)
{
    uint32_t reg_val, val;
    assert(dac != NULL);

    // enable DAC (Left channel) and/or DAC (Right channel)
    reg_val = dac->reg->REG_AUD_R21_DAC_CTRL6.all;
    val = dev_bmp & DAC_BMP_STEREO;
//    if (val == DAC_BMP_STEREO) {
//        if ((reg_val & R21_DACLR_EN) != R21_DACLR_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val | R21_DACLR_EN;
//    } else if (val == DAC_BMP_LEFT) {
//        if ((reg_val & R21_DACL_EN) != R21_DACL_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val | R21_DACL_EN;
//    } else if (val == DAC_BMP_RIGHT) {
//        if ((reg_val & R21_DACR_EN) != R21_DACR_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val | R21_DACR_EN;
//    } else {
//        CLOGW("%s: invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
//        return;
//    }

    if (val == DAC_BMP_STEREO || val == DAC_BMP_RIGHT) {
        CLOGW("%s: ONLY 1 DAC, invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
        return;
    } else { // if (val == DAC_BMP_LEFT)
        if ((reg_val & R21_DAC_EN) != R21_DAC_EN)
            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val | R21_DAC_EN;
    }

    // enable DAC internal clock
    reg_val = dac->reg->REG_AUD_R15_DAC_CTRL0.all;
    if (!(reg_val & R15_DACCLK_EN)) {
        dac->reg->REG_AUD_R15_DAC_CTRL0.all = reg_val | R15_DACCLK_EN;
    }
}

// disable DAC (digital & analog) and its internal clock
static void disable_dac(DAC_GRP *dac, uint8_t dev_bmp)
{
    uint32_t reg_val, val;
    assert(dac != NULL);

    // disable DAC (Left channel) and/or DAC (Right channel)
    reg_val = dac->reg->REG_AUD_R21_DAC_CTRL6.all;
    val = dev_bmp & DAC_BMP_STEREO;
//    if (val == DAC_BMP_STEREO) {
//        if (reg_val & R21_DACLR_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val & ~R21_DACLR_EN;
//    } else if (val == DAC_BMP_LEFT) {
//        if (reg_val & R21_DACL_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val & ~R21_DACL_EN;
//    } else if (val == DAC_BMP_RIGHT) {
//        if (reg_val & R21_DACR_EN)
//            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val & ~R21_DACR_EN;
//    } else {
//        CLOGW("%s: invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
//        return;
//    }

    if (val == DAC_BMP_STEREO || val == DAC_BMP_RIGHT) {
        CLOGW("%s: ONLY 1 DAC, invalid dev_bmp (0x%x)\r\n", __func__, dev_bmp);
        return;
    } else { // if (val == DAC_BMP_LEFT)
        if (reg_val & R21_DAC_EN)
            dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_val & ~R21_DAC_EN;
    }

    // disable DAC internal clock if possible
    reg_val = dac->reg->REG_AUD_R15_DAC_CTRL0.all;
    if (reg_val & R15_DACCLK_EN) {
        // if neither DAC Left nor DAC Right is enabled
        //if (!(dac->reg->REG_AUD_R21_DAC_CTRL6.all & R21_DAC_EN))
        dac->reg->REG_AUD_R15_DAC_CTRL0.all = reg_val & ~R15_DACCLK_EN;
    }
}

static inline void enable_dac_clk(DAC_GRP *dac)
{
    //assert(dac != NULL);
#if (ARCS_VER < ARCS_D0_SOC) // B0, C0
    codec_clk_enable();
#else
    codec_dac_clk_enable(); // D0
#endif
}

static inline void disable_dac_clk(DAC_GRP *dac)
{
    //assert(dac != NULL);
#if (ARCS_VER < ARCS_D0_SOC) // B0, C0
    codec_clk_disable();
#else
    codec_dac_clk_disable(); // D0
#endif
}

//------------------------------------------------------------------------------------------

/**
 \fn          CSK_DRIVER_VERSION DAC_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
DAC_GetVersion()
{
    return dac_driver_version;
}


/**
 \fn          int32_t DAC_Initialize(void *dac_grp, ...)
 \brief       Initialize DAC device group. A DAC device group includes at most two DAC devices,
              i.e. DAC01 includes DAC0 (Left Channel) and DAC1 (Right Channel) two devices.
              A DAC device must belong to some DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   cb_event  Pointer to \ref CSK_DAC_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \param[in]   dev_bmp  which DAC devices (similar to "I2S channels") are used,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  which DAC devices' echo channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo channels of DAC0 & DAC1
 \param[in]   init_flags  see below
 \return      \ref execution_status
*/
//int32_t
//DAC_Initialize(void *dac_grp, CSK_DAC_SignalEvent_t cb_event, uint32_t usr_param,
//                uint8_t dev_bmp, uint8_t echo_bmp, uint8_t init_flags)
int32_t
DAC_Initialize(void *dac_grp, CSK_DAC_SignalEvent_t cb_event, uint32_t usr_param,
                uint8_t dev_bmp_flag, DAC_DMA_CHS *dma_chs_p)
{
    g_sysctrl = IP_SYSCTRL;
    g_codec = (AUDIO_CODEC_RegDef *)AP_CODEC_BASE;

    int32_t ret;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t dev_bmp = (dev_bmp_flag & DAC_BMP_FLAG_OUT_STEREO) >> DAC_BMP_FLAG_OUT_POS;
    uint8_t echo_bmp = (dev_bmp_flag & DAC_BMP_FLAG_ECHO_STEREO) >> DAC_BMP_FLAG_ECHO_POS;

    if (dac == NULL || dma_chs_p == NULL || dev_bmp != CH_BMP_LEFT ||
        (echo_bmp != 0 && echo_bmp != CH_BMP_LEFT)) {
        CLOGW("%s: invalid initialize parameter!", __func__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

//    if (echo_bmp != 0 && dac->dev_idx != 0) {
//        CLOGW("%s: only DAC01 supports soft-ECHO channel!", __func__);
//        return CSK_DRIVER_ERROR_PARAMETER;
//    }

    if (dac->info->flags & DAC_FLAG_INITIALIZED)
        return CSK_DAC_ERROR_INITED_ALREADY;

    // initialize APC
    ret = apc_initialize();
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: failed to call apc_initialize()!", __func__);
        return ret;
    }

    // acquire APC channels of DAC
    ret = apc_dual_channel_acquire(dac->apc_dch, APC_INTF_DAC, dac->dev_idx,
                            (uint8_t *)dma_chs_p, dac_apc_event, (uint32_t)dac);
    if (ret != CSK_DRIVER_OK) {
        LOGD("%s: failed to acquire APC channels of DAC!", __func__);
        return ret;
    }

    // acquire APC channels of ECHO
    if (echo_bmp & CH_BMP_STEREO) {
        ret = apc_dual_channel_acquire(dac->dch_echo, APC_INTF_ECHO, APC_INTF_IDX_ECHO0, //FIXME: APC_INTF_IDX_ECHO1?
                        ((uint8_t *)dma_chs_p) + 2, dac_apc_event, (uint32_t)dac);
        if (ret != CSK_DRIVER_OK) {
            LOGD("%s: failed to acquire APC channels of ECHO!", __func__);
            return ret;
        }
    }

    // initialize DAC run-time resources
    memset(dac->info, 0, sizeof(DAC_INFO));
    dac->info->cb_event = cb_event;
    dac->info->usr_param = usr_param;
    dac->info->use_16bits = (dev_bmp_flag & DAC_BMP_FLAG_USE_16BITS) ? 1 : 0;
    dac->info->ch_bmp = dev_bmp;
    dac->info->echo_bmp = echo_bmp;
    dac->info->status.all = 0U;

    //TODO: other initialization...

    dac->info->flags = DAC_FLAG_INITIALIZED; // DAC is initialized
    LOGD("%s: DAC version: API = 0x%x, DRV = 0x%x\n",
            __func__, dac_driver_version.api, dac_driver_version.drv);

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t DAC_Uninitialize(void *dac_grp)
 \brief       De-initialize DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \return      \ref execution_status
*/
int32_t
DAC_Uninitialize(void *dac_grp)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL) {
        CLOGW("%s: invalid DAC group (0x%08x)!", __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((dac->info->flags & DAC_FLAG_INITIALIZED) == 0)
        return CSK_DRIVER_ERROR;

    // Abort current tranfers if any
    //DAC_Abort(dac_grp, dac->info->ch_bmp, dac->info->echo_bmp);

    //TODO: any other clean operations?

    // Power off DAC if Powered on
    if (dac->info->flags & DAC_FLAG_POWERED)
        DAC_PowerControl(dac_grp, CSK_POWER_OFF);

    // Release APC channels of DAC and/or ECHO
    assert(dac->info->ch_bmp != 0);
    apc_dual_channel_release(dac->apc_dch);
    if (dac->info->echo_bmp != 0) {
        apc_dual_channel_release(dac->dch_echo);
    }

    // Uninitialize APC
    apc_uninitialize();

    dac->info->flags = 0U; // DAC is uninitialized
    LOGD("%s is called for DAC group %d!\n", __func__, dac->dev_idx);
    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t DAC_PowerControl(void *dac_grp, ...)
 \brief       Control DAC device group's Power.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
DAC_PowerControl(void *dac_grp, CSK_POWER_STATE state)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL) {
        CLOGW("%s: invalid DAC group (0x%08x)!", __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((dac->info->flags & DAC_FLAG_INITIALIZED) == 0)
        return CSK_DRIVER_ERROR;

    uint32_t val;
    switch (state) {
    case CSK_POWER_OFF:

        // abort current TX and clean state if configured
        if (dac->info->flags & DAC_FLAG_INITIALIZED)
            DAC_Abort(dac_grp, dac->info->ch_bmp, dac->info->echo_bmp);

        // disable DAC module
        disable_dac(dac, dac->info->ch_bmp);

        // disable DAC clock -- the clock is shared with other dac or i2s
        // FIXME: is it correct to disable the clock?
        disable_dac_clk(dac);

        //TODO: other power-off configuration...

        dac->info->flags &= ~(DAC_FLAG_POWERED | DAC_FLAG_CONFIGURED);
        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:

        if ((dac->info->flags & DAC_FLAG_POWERED) != 0U)
            return CSK_DRIVER_OK;

        // enable DAC clock here
        enable_dac_clk(dac);

        // remove unnecessary disable operation, and it might cause some issue?
        //disable_dac(dac, dac->info->ch_bmp);

        // NOTE: enable DAC in advance when powered on, or else it may
        // push out some garbage data during DAC initialization?
        enable_dac(dac, dac->info->ch_bmp);

//        // enable DAC clock here
//        enable_dac_clk(dac);

        //TODO: other power-on configuration...

        dac->info->flags |= DAC_FLAG_POWERED; // DAC is powered
        break;

    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

//NOTE: ONLY 1 DAC on ARCS, dev_bmp CAN be CH_BMP_LEFT ONLY!
int32_t dac_enable_tx_channels(DAC_GRP *dac, uint8_t dev_bmp)
{
    int32_t ret = CSK_DRIVER_ERROR;

    assert(dac != NULL && dev_bmp != 0);
    if ((dev_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        ret = apc_dual_channel_enable(dac->apc_dch);
    } else if (dev_bmp & CH_BMP_LEFT) {
        ret = apc_channel_enable(APC_DCH_TO_CH(dac->apc_dch, 0));
    } else if (dev_bmp & CH_BMP_RIGHT) {
        ret = apc_channel_enable(APC_DCH_TO_CH(dac->apc_dch, 1));
    } else {
        assert(0);
    }

    return ret;
}

//NOTE: ONLY 1 DAC on ARCS, dev_bmp CAN be CH_BMP_LEFT ONLY!
static int32_t dac_disable_tx_channels(DAC_GRP *dac, uint8_t dev_bmp)
{
    int32_t ret = CSK_DRIVER_ERROR;

    assert(dac != NULL && dev_bmp != 0);
    if ((dev_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        ret = apc_dual_channel_disable(dac->apc_dch);
    } else if (dev_bmp & CH_BMP_LEFT) {
        ret = apc_channel_disable(APC_DCH_TO_CH(dac->apc_dch, 0));
    } else if (dev_bmp & CH_BMP_RIGHT) {
        ret = apc_channel_disable(APC_DCH_TO_CH(dac->apc_dch, 1));
    } else {
        assert(0);
    }

    return ret;
}


/*
static inline void clear_dac_data_dup() {
    uint32_t reg_val;
    reg_val = inw(DAC_DATA_DUP_REG);
    if ((reg_val & DAC_DATA_DUP_MASK) != DAC_DATA_DUP_NONE) {
        reg_val &= ~DAC_DATA_DUP_MASK;
        outw(DAC_DATA_DUP_REG, reg_val);
    }
}

static inline void set_dac_data_dup() {
    uint32_t reg_val;
    reg_val = inw(DAC_DATA_DUP_REG);
    if ((reg_val & DAC_DATA_DUP_MASK) != DAC_DATA_DUP_LEFT) {
        reg_val &= ~DAC_DATA_DUP_MASK;
        reg_val |= DAC_DATA_DUP_LEFT;
        outw(DAC_DATA_DUP_REG, reg_val);
    }
}
*/


/**
 \fn          int32_t DAC_Send(void *dac_grp, ...)
 \brief       Send data within the buffer
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   data  Pointer to buffer of data to send
 \param[in]   num   Number of data items to receive
 \param[in]   dev_bmp  send data from which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   tx_flag   bit flags of send operation
 \return      \ref execution_status
*/
int32_t
DAC_Send_Body(void *dac_grp, const uint32_t *data, uint32_t num,
            uint32_t buf_offset, uint32_t src_gath,
            uint8_t dev_bmp, uint8_t tx_flag)
{
    int32_t ret;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t bmp = 0;

    //NOTE: ONLY 1 DAC on ARCS, dev_bmp CAN be CH_BMP_LEFT ONLY!
    if (dac == NULL || (bmp = dac->info->ch_bmp & dev_bmp) != CH_BMP_LEFT) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: %d",
                __func__, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (dac->info->status.bit.busy & bmp)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t start_now = tx_flag & DAC_TX_FLAG_START_NOW;
    if (!start_now) {
        ret = dac_disable_tx_channels(dac, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    uint8_t nsynca = (tx_flag & DAC_TX_FLAG_NSYNCA) ? 1 : 0;
    uint8_t lch = APC_DCH_TO_CH(dac->apc_dch, 0);
    ret = CSK_DRIVER_ERROR;

/*
    // clear data_duplicate bit if stereo source or mono channel output
    if (dac->info->src_stereo || (bmp & CH_BMP_STEREO) != CH_BMP_STEREO) {
        clear_dac_data_dup();
    }
*/

/*
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (dac->info->nsynca_l != nsynca) { // use left channel for both
            apc_channel_set_nsynca(lch, nsynca);
            dac->info->nsynca_l = nsynca;
        }
        if (dac->info->src_stereo) { // stereo source
            ret = apc_dual_channel_write(dac->apc_dch, data, num, buf_offset, src_gath);
        } else { // mono source
            set_dac_data_dup();
            ret = apc_channel_write(lch, data, num, buf_offset, src_gath);
        }
    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT) {
        if (dac->info->nsynca_l != nsynca) { // use left channel
            apc_channel_set_nsynca(lch, nsynca);
            dac->info->nsynca_l = nsynca;
        }
        ret = apc_channel_write(lch, data, num, buf_offset, src_gath);
    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT) {
        uint8_t rch = APC_DCH_TO_CH(dac->apc_dch, 1);
        if (dac->info->nsynca_r != nsynca) { // use right channel
            apc_channel_set_nsynca(rch, nsynca);
            dac->info->nsynca_r = nsynca;
        }
        ret = apc_channel_write(rch, data, num, buf_offset, src_gath);
    }
*/

    //NOTE: ONLY 1 DAC (Left channel) on ARCS!
    if (dac->info->nsynca_l != nsynca) { // use left channel
        apc_channel_set_nsynca(lch, nsynca);
        dac->info->nsynca_l = nsynca;
    }
    ret = apc_channel_write(lch, data, num, buf_offset, src_gath);
    if (ret != CSK_DRIVER_OK)
        return ret;

    dac->info->status.bit.busy |= bmp;
    dac->info->status.bit.tx_emp = 0;
    dac->info->status.bit.tx_undf = 0;

    if (start_now) {
        //enable_dac(dac, bmp); // enable DAC module
        ret = dac_enable_tx_channels(dac, bmp);
    }

    return ret;
}

int32_t
DAC_Send(void *dac_grp, const uint32_t *data, uint32_t num,
        uint8_t dev_bmp, uint8_t tx_flag)
{
    return DAC_Send_Body(dac_grp, data, num, 0, 0, dev_bmp, tx_flag);
}


// Send data via DAC interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_OUT_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
DAC_Send_PiPo(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t dev_bmp, uint8_t tx_flag)
{
    int32_t ret;
    uint8_t bmp = 0;
    //uint8_t full_chk = !(tx_flag & DAC_TX_FLAG_QUICK_CHECK);

    DAC_GRP *dac = safe_dac_grp(dac_grp);
    //NOTE: ONLY 1 DAC on ARCS, dev_bmp CAN be CH_BMP_LEFT ONLY!
    if (dac == NULL || (bmp = dac->info->ch_bmp & dev_bmp) != CH_BMP_LEFT ||
        blks == NULL || blk_cnt_p == NULL) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: %d",
                __func__, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t started = dac->info->status.bit.busy & bmp;
    uint8_t start_now = 0;

    if (!started) {
        if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
            LOGD("%s: DAC group %d has NOT been configured!!",
                    __func__, dac->dev_idx);
            return CSK_DRIVER_ERROR;
        }

        //if (dac->info->status.bit.busy & bmp)
        //    return CSK_DRIVER_ERROR_BUSY;

        start_now = tx_flag & DAC_TX_FLAG_START_NOW;
        if (!start_now) {
            ret = dac_disable_tx_channels(dac, bmp);
            if (ret != CSK_DRIVER_OK)
                return ret;
        }

        //NOTE: ONLY 1 DAC (Left channel) on ARCS!
        uint8_t nsynca = (tx_flag & DAC_TX_FLAG_NSYNCA) ? 1 : 0;
        uint8_t lch = APC_DCH_TO_CH(dac->apc_dch, 0);
        if (dac->info->nsynca_l != nsynca) { // use left channel
            apc_channel_set_nsynca(lch, nsynca);
            dac->info->nsynca_l = nsynca;
        }
    } // !started

    ret = apc_dch_write_pipo(dac->apc_dch, dev_bmp, blks, blk_cnt_p, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        dac->info->status.bit.busy |= bmp;
        dac->info->status.bit.tx_emp = 0;
        dac->info->status.bit.tx_undf = 0;

        if (start_now) {
            //enable_dac(dac, bmp); // enable DAC module
            ret = dac_enable_tx_channels(dac, bmp);
        }
    }

    return ret;
}


// return count of transferred block if >= 0, else return the error value.
int32_t
DAC_PiPo_Xferred_Blocks(void *dac_grp, PIPO_OUT_BLOCK *blks, uint8_t blk_cnt, uint8_t dev_bmp)
{
    int32_t ret;
    uint8_t bmp;
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    if (dac == NULL ||  (bmp = dac->info->ch_bmp & dev_bmp) != CH_BMP_LEFT ||
        blks == NULL || blk_cnt == 0) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x", __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED) || !dac->info->status.bit.busy) {
        CLOGW("%s: DAC has NOT been configured or started!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    return apc_dch_get_pipo_blks(dac->apc_dch, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
}


/**
 \fn          int32_t DAC_Send_LLP(void *dac_grp, ...)
 \brief       Send data within several non-continuous buffers
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   bufs  Pointer to list of buffers of data to send
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   dev_bmp  send data from which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   tx_flag   bit flags of send operation
 \return      \ref execution_status
*/
/*
int32_t
DAC_Send_LLP_Body(void *dac_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt,
            uint32_t buf_offset, uint32_t src_gath,
            uint8_t dev_bmp, uint8_t tx_flag)
{
    int32_t ret;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t bmp = 0;

    if (dac == NULL || (bmp = dac->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: %d",
                __func__, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        CLOGW("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (dac->info->status.bit.busy & bmp)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t start_now = tx_flag & DAC_TX_FLAG_START_NOW;
    if (!start_now) {
        ret = dac_disable_tx_channels(dac, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    uint8_t nsynca = (tx_flag & DAC_TX_FLAG_NSYNCA) ? 1 : 0;
    uint8_t lch = APC_DCH_TO_CH(dac->apc_dch, 0);

    ret = CSK_DRIVER_ERROR;

    // clear data_duplicate bit if stereo source or mono channel output
    if (dac->info->src_stereo || (bmp & CH_BMP_STEREO) != CH_BMP_STEREO) {
        clear_dac_data_dup();
    }

    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (dac->info->nsynca_l != nsynca) { // use left channel for both
            apc_channel_set_nsynca(lch, nsynca);
            dac->info->nsynca_l = nsynca;
        }
        if (dac->info->src_stereo) { // stereo source
            ret = apc_dual_channel_write_LLP(dac->apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
        } else { // mono source
            set_dac_data_dup();
            ret = apc_channel_write_LLP(lch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
        }
    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT) {
        if (dac->info->nsynca_l != nsynca) { // use left channel
            apc_channel_set_nsynca(lch, nsynca);
            dac->info->nsynca_l = nsynca;
        }
        ret = apc_channel_write_LLP(lch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT) {
        uint8_t rch = APC_DCH_TO_CH(dac->apc_dch, 1);
        if (dac->info->nsynca_r != nsynca) { // use right channel
            apc_channel_set_nsynca(rch, nsynca);
            dac->info->nsynca_r = nsynca;
        }
        ret = apc_channel_write_LLP(rch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
    }

    if (ret != CSK_DRIVER_OK)
        return ret;

    dac->info->status.bit.busy |= bmp;
    dac->info->status.bit.tx_emp = 0;
    dac->info->status.bit.tx_undf = 0;

    if (start_now) {
        //enable_dac(dac, bmp); // enable DAC module
        ret = dac_enable_tx_channels(dac, bmp);
     }

    return ret;
}

int32_t
DAC_Send_LLP(void *dac_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt,
            uint8_t dev_bmp, uint8_t tx_flag)
{
    return DAC_Send_LLP_Body(dac_grp, bufs, buf_cnt, 0, 0, dev_bmp, tx_flag);
}
*/


//NOTE: ONLY 1 DAC (Left channel) on ARCS, so There's ONLY 1 ECHO channel!
int32_t dac_enable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp)
{
    int32_t ret = CSK_DRIVER_ERROR;

    assert(dac != NULL && echo_bmp != 0);
    if ((echo_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        ret = apc_dual_channel_enable(dac->dch_echo);
    } else if (echo_bmp & CH_BMP_LEFT) {
        ret = apc_channel_enable(APC_DCH_TO_CH(dac->dch_echo, 0));
    } else if (echo_bmp & CH_BMP_RIGHT) {
        ret = apc_channel_enable(APC_DCH_TO_CH(dac->dch_echo, 1));
    } else {
        assert(0);
    }

    return ret;
}

 //NOTE: ONLY 1 DAC (Left channel) on ARCS, so There's ONLY 1 ECHO channel!
int32_t dac_disable_echo_channels(DAC_GRP *dac, uint8_t echo_bmp)
{
    int32_t ret = CSK_DRIVER_ERROR;

    assert(dac != NULL && echo_bmp != 0);
    if ((echo_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        ret = apc_dual_channel_disable(dac->dch_echo);
    } else if (echo_bmp & CH_BMP_LEFT) {
        ret = apc_channel_disable(APC_DCH_TO_CH(dac->dch_echo, 0));
    } else if (echo_bmp & CH_BMP_RIGHT) {
        ret = apc_channel_disable(APC_DCH_TO_CH(dac->dch_echo, 1));
    } else {
        assert(0);
    }

    return ret;
}

/**
 \fn          int32_t DAC_Echo_Receive(void *dac_grp, ...)
 \brief       Receive echo data into the buffer
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   data  Pointer to buffer to receive echo data
 \param[in]   num   Number of data items to receive
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
//NOTE: ONLY 1 ECHO channel (corresponding to the only DAC @ Left channel) on ARCS, echo_bmp CAN be CH_BMP_LEFT ONLY!
int32_t
DAC_Echo_Receive(void *dac_grp, uint32_t *data, uint32_t num, uint8_t echo_bmp)
{
    int32_t ret;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || (echo_bmp & CH_BMP_STEREO) != CH_BMP_LEFT) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, ECHO channel_bmp: %d",
                __func__, dac_grp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t bmp = (dac->info->echo_bmp & echo_bmp);
    if (bmp != echo_bmp) {
        LOGD("%s: digital ECHO is NOT supported by DAC group %d, or echo channel_bmp 0x%x is NOT configured!!",
                __func__, dac->dev_idx, echo_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (dac->info->status.bit.ech_busy & bmp)
        return CSK_DRIVER_ERROR_BUSY;

    ret = CSK_DRIVER_ERROR;
    APC_DCH apc_dch = dac->dch_echo;
/*
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read(apc_dch, data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 0), data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 1), data, num, 0, 0);
*/
    //NOTE: ONLY 1 ECHO (corresponding to the only DAC @Left channel) on ARCS!
    ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 0), data, num, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    dac->info->status.bit.ech_busy |= bmp;
    dac->info->status.bit.ech_ovf = 0;
    ret = dac_enable_echo_channels(dac, bmp);

    //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
    if (ret == CSK_DRIVER_OK) {
        ret = dac_enable_tx_channels(dac, bmp);
        //enable_dac(dac, bmp); // enable DAC device group
    }

    return ret;
}


// Receive echo data in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_IN_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
DAC_Echo_Receive_PiPo(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p, uint8_t echo_bmp)
{
    int32_t ret;
    uint8_t bmp = 0;

    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || (bmp = dac->info->echo_bmp & echo_bmp) != CH_BMP_LEFT ||
        blks == NULL || blk_cnt_p == NULL) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, ECHO channel_bmp: %d",
                __func__, dac_grp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t started = dac->info->status.bit.ech_busy & bmp;
    //uint8_t start_now = 1; //0;

    if (!started) {
        if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
            LOGD("%s: DAC group %d has NOT been configured!!",
                    __func__, dac->dev_idx);
            return CSK_DRIVER_ERROR;
        }

        //TODO: add other initial check / operation...

    } // !started

    //ret = CSK_DRIVER_ERROR;

    //NOTE: ONLY 1 ECHO (corresponding to the only DAC @Left channel) on ARCS!
    ret = apc_dch_read_pipo(dac->dch_echo, bmp, blks, blk_cnt_p, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        dac->info->status.bit.ech_busy |= bmp;
        dac->info->status.bit.ech_ovf = 0;
        ret = dac_enable_echo_channels(dac, bmp);

        //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
        if (ret == CSK_DRIVER_OK) {
            ret = dac_enable_tx_channels(dac, bmp);
            //enable_dac(dac, bmp); // enable DAC device group
        }
    } // !started

    return ret;
}


//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
DAC_Echo_PiPo_Xferred_Blocks(void *dac_grp, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t echo_bmp)
{
    int32_t ret;
    uint8_t bmp;
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    if (dac == NULL ||  (bmp = dac->info->echo_bmp & echo_bmp) != CH_BMP_LEFT ||
        blks == NULL || blk_cnt == 0) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x", __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED) || !(dac->info->status.bit.ech_busy & bmp)) {
        CLOGW("%s: DAC has NOT been configured or started!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    return apc_dch_get_pipo_blks(dac->dch_echo, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
}


/**
 \fn          int32_t DAC_Echo_Receive_LLP(void *dac_grp, ...)
 \brief       Receive echo data into several non-continuous buffers
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   bufs  Pointer to list of buffer to receive echo data
 \param[in]   buf_cnt  Number of data buffers in the list
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
/*
int32_t
DAC_Echo_Receive_LLP(void *dac_grp, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t echo_bmp)
{
    int32_t ret;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || (echo_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, ECHO channel_bmp: %d",
                __func__, dac_grp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t bmp = (dac->info->echo_bmp & echo_bmp);
    if (bmp != echo_bmp) {
        LOGD("%s: softECHO is NOT supported by DAC group %d, or echo channel_bmp 0x%x is NOT configured!!",
                __func__, dac->dev_idx, echo_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    ret = CSK_DRIVER_ERROR;
    APC_DCH apc_dch = dac->dch_echo;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read_LLP(apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 0), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 1), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    dac->info->status.bit.ech_ovf = 0;
    ret = dac_enable_echo_channels(dac, bmp);

    //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
    if (ret == CSK_DRIVER_OK) {
        ret = dac_enable_tx_channels(dac, bmp);
        //enable_dac(dac, bmp); // enable DAC device group
    }

    return ret;
}
*/


/**
 \fn          int32_t DAC_Enable(void *dac_grp, ...)
 \brief       Enable DAC device(s) and data send is started
              if DAC_Send_XXX is called before and START_NOW NOT specified.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
int32_t
DAC_Enable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    dev_bmp &= dac->info->ch_bmp;
    echo_bmp &= dac->info->echo_bmp;

    if (dac == NULL || (!dev_bmp && !echo_bmp)) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: 0x%x, dev_bmp: 0x%x",
                __func__, dac_grp, dev_bmp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (dev_bmp != 0) {
        ret = dac_enable_tx_channels(dac, dev_bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    if (echo_bmp != 0) {
        ret = dac_enable_echo_channels(dac, echo_bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    //if (ret == CSK_DRIVER_OK)
    //    enable_dac(dac, bmp); // enable DAC module

    return ret;
}


/**
 \fn          int32_t DAC_Disable(void *dac_grp, ...)
 \brief       Disable DAC device(s) (and data send is suspended if any).
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group,
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  receive data from which echo channels of DAC device group,
              Left Echo Channel @bit[0] and Right Echo Channel @bit[1],
              and echo_bmp = 0x3 indicates both echo channels of DAC device group
 \return      \ref execution_status
*/
int32_t
DAC_Disable(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    dev_bmp &= dac->info->ch_bmp;
    echo_bmp &= dac->info->echo_bmp;

    if (dac == NULL || (!dev_bmp && !echo_bmp)) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: 0x%x, dev_bmp: 0x%x",
                __func__, dac_grp, dev_bmp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (dev_bmp != 0) {
        ret = dac_disable_tx_channels(dac, dev_bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    if (echo_bmp != 0) {
        ret = dac_disable_echo_channels(dac, echo_bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    //if (ret == CSK_DRIVER_OK)
    //    disable_dac(dac, bmp); // disable DAC module

    return ret;
}


/**
 \fn          int32_t DAC_Abort(void *dac_grp, ...)
 \brief       Abort DAC data transfer if any.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \param[in]   echo_bmp  which DAC devices' echo data channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo data channels of DAC0 & DAC1
 \return      \ref execution_status
*/
int32_t
DAC_Abort(void *dac_grp, uint8_t dev_bmp, uint8_t echo_bmp)
{
    int32_t ret = CSK_DRIVER_OK;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t bmp = 0;
    uint8_t bmp2 = 0;

    if (dac == NULL || ((bmp = dac->info->ch_bmp & dev_bmp) == 0 &&
                        (bmp2 = dac->info->echo_bmp & echo_bmp) == 0)) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: 0x%x, echo_bmp: 0x%x",
                __func__, dac_grp, dev_bmp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (bmp != 0) { // OUT channels
/*
        if (dac->info->ch_mix || bmp == CH_BMP_STEREO)
            ret = apc_dual_channel_abort(dac->apc_dch);
        else if (bmp == CH_BMP_LEFT)
            ret = apc_channel_abort(APC_DCH_TO_CH(dac->apc_dch, 0));
        else if (bmp == CH_BMP_RIGHT)
            ret = apc_channel_abort(APC_DCH_TO_CH(dac->apc_dch, 1));
*/
        ret = apc_channel_abort(APC_DCH_TO_CH(dac->apc_dch, 0));
        if (ret != CSK_DRIVER_OK)
            return ret;
        dac->info->status.bit.busy &= ~bmp;
    }

    if (bmp2 != 0) { // ECHO channels
/*
        if (dac->info->ch_mix || bmp == CH_BMP_STEREO)
            ret = apc_dual_channel_abort(dac->dch_echo);
        else if (bmp == CH_BMP_LEFT)
            ret = apc_channel_abort(APC_DCH_TO_CH(dac->dch_echo, 0));
        else if (bmp == CH_BMP_RIGHT)
            ret = apc_channel_abort(APC_DCH_TO_CH(dac->dch_echo, 1));
*/
        ret = apc_channel_abort(APC_DCH_TO_CH(dac->dch_echo, 0));
        if (ret != CSK_DRIVER_OK)
            return ret;
        dac->info->status.bit.ech_busy &= ~bmp2;
    }

    return ret;
}


/**
 \fn          uint32_t DAC_GetTxCount(void *dac_grp, ...)
 \brief       Get count of data sent from DAC device(s).
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   dev_bmp  specify which DAC devices in the device group
              DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
              and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \return      number of data items (24-bit samples)transferred if positive, error value if negative.
*/
int32_t
DAC_GetTxCount(void *dac_grp, uint8_t dev_bmp)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t bmp = 0;

    if (dac == NULL || (bmp = dac->info->ch_bmp & dev_bmp) == 0) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: %d",
                __func__, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        CLOGW("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

/*
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (dac->info->ch_mix)
            return apc_dual_channel_get_count(dac->apc_dch);

        int32_t ret = 0;
        if (dac->info->ch_bmp & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(dac->apc_dch, 0));
        if (dac->info->ch_bmp & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(dac->apc_dch, 1));
        return ret;

    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(dac->apc_dch, 0));
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(dac->apc_dch, 1));

    return CSK_DRIVER_ERROR;
*/
    return apc_channel_get_count(APC_DCH_TO_CH(dac->apc_dch, 0));
}


/**
 \fn          uint32_t DAC_GetEchoCount(void *dac_grp, ...)
 \brief       Get transferred echo data count.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   echo_bmp  which DAC devices' echo data channels are used,
              DAC0 echo (Left Channel, @bit[0]) and DAC1 echo (Right Channel, @bit[1]),
              and echo_bmp = 0x3 indicates both echo data channels of DAC0 & DAC1
 \return      number of data items transferred if positive,
              error value if negative.
*/
int32_t
DAC_GetEchoCount(void *dac_grp, uint8_t echo_bmp)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || (echo_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, DAC group: 0x%08x, ECHO channel_bmp: %d",
                __func__, dac_grp, echo_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((dac->info->echo_bmp & echo_bmp) != echo_bmp) {
        LOGD("%s: softECHO is NOT supported by DAC group %d, or echo channel_bmp 0x%x is NOT configured!!",
                __func__, dac->dev_idx, echo_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH apc_dch = dac->dch_echo;
/*
    if ((echo_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (dac->info->mix_echo)
            return apc_dual_channel_get_count(apc_dch);

        int32_t ret = 0;
        if (dac->info->echo_bmp & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
        if (dac->info->echo_bmp & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));
        return ret;

    } else if ((echo_bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
    else if ((echo_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));

    return CSK_DRIVER_ERROR;
*/
    return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
}


static int32_t dac_get_samp_rate(DAC_GRP *dac)
{
    uint32_t i, reg_val;
    assert(dac != NULL);

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        CLOGW("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (dac->info->samp_freq != 0)
        return dac->info->samp_freq;

    reg_val = dac->reg->REG_AUD_R15_DAC_CTRL0.bit.DACSR;
    for (i = 0; i < ARRAY_COUNT(sc_DAC_SR_val_map); i++) {
        if (reg_val == sc_DAC_SR_val_map[i].reg_val)
            break;
    }
    if (i >= ARRAY_COUNT(sc_DAC_SR_val_map)) {
        CLOGW("%s: Sample rate's reg_val (%d) is NOT in built-in supported freq list!!\r\n",
                __func__, reg_val);
        return CSK_DRIVER_ERROR;
    }

    return sc_DAC_SR_val_map[reg_val].real_val;
}


static int32_t dac_set_echo_params(DAC_GRP *dac, ECHO_PARAMS *params)
{
    int32_t ret;

    assert(dac != NULL);
    // ONLY 1 ECHO (Left) channel corresponding to the only DAC on ARCS!
    if (params == NULL || params->echo_mixed == 1) {
        LOGD("%s: invalid ECHO parameters!\n", __func__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (dac->info->echo_bmp == 0) {
        LOGD("%s: SHOULD acquire ECHO channels (echo_bmp=%x) before this!\n",
                __func__, dac->info->echo_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (dac->info->samp_freq == 0 || dac->data_len == 0) {
        LOGD("%s: SHOULD set samp_freq (%d) before this!\n",
                __func__, dac->info->samp_freq);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH apc_dch = dac->dch_echo;
    ret = apc_dual_channel_setup(apc_dch, APC_CHMODE_24BITS_HIGH,
                                dac->info->echo_bmp,
                                params->echo_mixed,
                                params->trim_16bits);
    if (ret != CSK_DRIVER_OK)
        return ret;

    ret = apc_echo_setup(apc_dch,
                        dac->info->samp_freq,
                        params->samp_rate,
                        1, dac->info->echo_bmp); // support auto_feed
    if (ret != CSK_DRIVER_OK)
        return ret;

    //dac->info->mix_echo = params->echo_mixed ? 1 : 0;
    dac->info->mix_echo = params->echo_mixed;

    return CSK_DRIVER_OK;
}


static bool check_sr_osr_pair(DAC_GRP *dac)
{
    uint32_t i, count;

    assert(dac != NULL);
    count = ARRAY_COUNT(sc_DAC_SR_OSR);
    for (i=0; i<count; i++) {
        if (sc_DAC_SR_OSR[i].major == dac->info->samp_freq &&
                sc_DAC_SR_OSR[i].minor == dac->info->over_samp_ratio) {
            return true;
        }
    } // end for

    return false;
}

/**
 \fn          int32_t DAC_Control(void *dac_grp, ...)
 \brief       Control DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   control  Operation
 \param[in]   arg  Argument of operation (optional), i.e. sample rate
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/
int32_t
DAC_Control(void *dac_grp, uint32_t control, uint32_t arg)
{
    int32_t ret, val;
    uint32_t mclk_flag, tx_cfg;
    union AUD_R15_DAC_CTRL0 reg_r15;
    union AUD_R17_DAC_CTRL2 reg_r17;
    union AUD_R18_DAC_CTRL3 reg_r18;
    union AUD_R21_DAC_CTRL6 reg_r21;
    union AUD_R22_DAC_CTRL7 reg_r22;

    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL) {
        CLOGW("%s: invalid DAC group (0x%08x)!", __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_POWERED))
        return CSK_DRIVER_ERROR;

    // exclusive MISC OP
    switch (control & CSK_DAC_EXCL_OP_Msk) {
        // NO MISC OP
        case CSK_DAC_EXCL_OP_UNSET:
            break;

        // abort DAC transfer
        case CSK_DAC_ABORT_TRANSFER:
            DAC_Abort(dac, (arg & DAC_BMP_STEREO), ((arg >> 8) & CH_BMP_STEREO));
            return CSK_DRIVER_OK;

        // get sample rate
        case CSK_DAC_GET_SAMP_RATE:
            return dac_get_samp_rate(dac);

        // Set ECHO parameters
        case CSK_DAC_SET_ECHO_PARAMS:
            return dac_set_echo_params(dac, (ECHO_PARAMS*)arg);

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    } // end exclusive MISC_OP

    if (dac->info->status.bit.busy)
        return CSK_DRIVER_ERROR_BUSY;

    reg_r15.all = dac->reg->REG_AUD_R15_DAC_CTRL0.all;
    reg_r17.all = dac->reg->REG_AUD_R17_DAC_CTRL2.all;
    reg_r18.all = dac->reg->REG_AUD_R18_DAC_CTRL3.all;
    reg_r21.all = dac->reg->REG_AUD_R21_DAC_CTRL6.all;
    reg_r22.all = dac->reg->REG_AUD_R22_DAC_CTRL7.all;

//    // Main Clock Selection (ALSO NEED SET AUDPLL...)
//    switch (control & CSK_DAC_MCLK_SEL_Msk) {
//    // Keep Main clock unchanged
//    case CSK_DAC_MCLK_SEL_UNSET:
//        break;
//
//    case CSK_DAC_MCLK_SEL_24MHZ:
//        reg_r15.bit.AUD_DAC_MCLK_SEL = 0;
//        break;
//
//    case CSK_DAC_MCLK_SEL_22P05MHZ:
//        reg_r15.bit.AUD_DAC_MCLK_SEL = 1;
//        break;
//
//    default:
//        return CSK_DAC_ERROR_MCLK_SEL;
//    }

//    // 24MHz main clock by default
//    reg_r15.bit.AUD_DAC_MCLK_SEL = 0;
//    // How to set main clock? mclk_flag options:
//    // 0 = use 24MHz, 1 = use 22.05MHz, 2 = not changed
//    mclk_flag = 0;

    // Sample Rate
    switch (control & CSK_DAC_SR_Msk) {
    // Keep Sample Rate unchanged
    case CSK_DAC_SR_UNSET:
//        mclk_flag = 2; // not changed
        break;

    case CSK_DAC_SR_8KHZ:
        reg_r15.bit.DACSR = 0x0;
        dac->info->samp_freq = 8000;
        break;

//    case CSK_DAC_SR_11P025KHZ:
//        mclk_flag = 1; // use 22.05MHz
//        reg_r15.bit.AUD_DAC_MCLK_SRC = 1; // 22.05MHz main clock
//        reg_r15.bit.DACSR = 0x1;
//        dac->info->samp_freq = 11025;
//        break;

//    case CSK_DAC_SR_12KHZ:
//        reg_r15.bit.DACSR = 0x2;
//        dac->info->samp_freq = 12000;
//        break;

    case CSK_DAC_SR_16KHZ:
        reg_r15.bit.DACSR = 0x3;
        dac->info->samp_freq = 16000;
        break;

//    case CSK_DAC_SR_22P05KHZ:
//        mclk_flag = 1; // use 22.05MHz
//        reg_r15.bit.AUD_DAC_MCLK_SRC = 1; // 22.05MHz main clock
//        reg_r15.bit.DACSR = 0x4;
//        dac->info->samp_freq = 22050;
//        break;

    case CSK_DAC_SR_24KHZ:
        reg_r15.bit.DACSR = 0x5;
        dac->info->samp_freq = 24000;
        break;

    case CSK_DAC_SR_32KHZ:
        reg_r15.bit.DACSR = 0x6;
        dac->info->samp_freq = 32000;
        break;

//    case CSK_DAC_SR_44P1KHZ:
//        mclk_flag = 1; // use 22.05MHz
//        reg_r15.bit.AUD_DAC_MCLK_SRC = 1; // 22.05MHz main clock
//        reg_r15.bit.DACSR = 0x7;
//        dac->info->samp_freq = 44100;
//        break;

    case CSK_DAC_SR_48KHZ:
        reg_r15.bit.DACSR = 0x8;
        dac->info->samp_freq = 48000;
        break;

    case CSK_DAC_SR_96KHZ:
        reg_r15.bit.DACSR = 0x9;
        dac->info->samp_freq = 96000;
        break;

    default:
        return CSK_DAC_ERROR_SAMP_RATE;
    }

//    if (mclk_flag == 0) { // use 24Mhz
//        dac_clk_select(AUDIO_CLK_SRC_XTAL);
//    } else if (mclk_flag == 1) { // use 22.05Mhz
//        uint8_t div_n = BOARD_BOOTCLOCKRUN_AUDPLL_DIV_N;
//        uint8_t div_m = BOARD_BOOTCLOCKRUN_AUDPLL_DIV_M;
//        AUDIOPLL_InitPLL(div_n, div_m, BOARD_BOOTCLOCKRUN_AUDPLL_DIV_NF);
//        init_audpll_audio_clk(0); // DEF_AUDPLL_AUDIO_FREQ (22050000)
//        dac_clk_select(AUDIO_CLK_SRC_AUDPLL);
//    }

    // Over Sample Ratio
    switch (control & CSK_DAC_OSR_Msk) {
    // Keep Over Sample Ratio unchanged
    case CSK_DAC_OSR_UNSET:
        break;

    case CSK_DAC_OSR_250:
        reg_r15.bit.DACOSR = 0x0;
        dac->info->over_samp_ratio = 250;
        break;

    case CSK_DAC_OSR_125:
        reg_r15.bit.DACOSR = 0x1;
        dac->info->over_samp_ratio = 125;
        break;

    default:
        return CSK_DAC_ERROR_SAMP_RATE;
    }

    // check if the combination of Sample Rate & Over Sample Ratio are supported
    if ((control & CSK_DAC_SR_Msk) != CSK_DAC_SR_UNSET || (control & CSK_DAC_OSR_Msk) != CSK_DAC_OSR_UNSET) {
        if (!check_sr_osr_pair(dac)) {
            CLOGW("%s: unsupported pair of Sample Rate(%d) and Over Sample Ratio(%d)!\n",
                    __func__, dac->info->samp_freq, dac->info->over_samp_ratio);
            return CSK_DAC_ERROR_SR_OSR_PAIR;
        }
    }

    // Soft Mute Setting
    switch (control & CSK_DAC_SOFT_MUTE_Msk) {
    // Keep Soft Mute unchanged
    case CSK_DAC_SOFT_MUTE_UNSET:
    default:
        break;

    case CSK_DAC_SOFT_MUTE_SET:
        reg_r17.bit.SOFTMUTE_EN = (arg >> 4) & 0x1; // bit[4]
        reg_r17.bit.SOFT_SPEED = arg & 0xF; // bit[3:0]
        break;
    }

    // Auto Mute Setting
    switch (control & CSK_DAC_AUTO_MUTE_Msk) {
    // Keep Auto Mute unchanged
    case CSK_DAC_AUTO_MUTE_UNSET:
    default:
        break;

    case CSK_DAC_AUTO_MUTE_SET:
//        reg_r18.bit.DAC_SD_AMUTE_EN_L = reg_r18.bit.DAC_SD_AMUTE_EN_R = (arg >> 8) & 0x1; // bit[8]
        reg_r18.bit.DAC_SD_AMUTE_EN = (arg >> 8) & 0x1; // bit[8]
        val = (arg >> 5) & 0x7; // bit[7:5]
        if (val > R18_DAC_SDM_AMUTE_MAX)
            return CSK_DRIVER_ERROR_PARAMETER;
        reg_r18.bit.DAC_SD_AMUTE_TYPE = val;
        break;
    }

//    // TX config  (mono/stereo, mixed or not)
//    tx_cfg = control & CSK_DAC_TXCFG_Msk;
//    switch (tx_cfg) {
//    // Keep TX config unchanged or default
//    case CSK_DAC_TXCFG_UNSET:
//        break;
//
//    case CSK_DAC_TXCFG_MONO_SRC_MONO:
//    case CSK_DAC_TXCFG_STEREO_SRC_MONO:
//        dac->info->ch_mix = 0;
//        dac->info->src_stereo = 0;
//        break;
//
//    case CSK_DAC_TXCFG_STEREO_SRC_STEREO:
//        dac->info->ch_mix = 1;
//        dac->info->src_stereo = 1;
//        break;
//
//    default:
//        return CSK_DAC_ERROR_TXCFG;
//    }

    val = dac->info->flags & DAC_FLAG_CONFIGURED;

    // call apc_dual_channel_setup to notify APC channels
    // FIXME: should it support APC_CHMODE_24BITS_LOW?
    // FIXME: check other conditions to avoid unnecessary call of apc_dual_channel_setup?
    if ( val == 0 ) { // || tx_cfg != CSK_DAC_TXCFG_UNSET
    #if DAC_USE_PIO
        uint8_t ch_flag = (dac->info->use_16bits ? 1 : 0) | 0x2; //use PIO for TEST!
        LOGD("%s: used PIO to write TX FIFO!", __func__);
    #else
        uint8_t ch_flag = (dac->info->use_16bits ? 1 : 0);
    #endif

        ret = apc_dual_channel_setup(dac->apc_dch,
                                    APC_CHMODE_24BITS_HIGH,
                                    dac->info->ch_bmp,
                                    dac->info->ch_mix,
                                    ch_flag);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    //TODO: support other control codes...

    //------------------------------------------------
    // write settings into DAC registers
    //------------------------------------------------

    //val = dac->info->flags & DAC_FLAG_CONFIGURED;

//    if (dac->info->ch_bmp & DAC_BMP_LEFT) {
//        reg_r17.bit.DACL_DWAEN = 1;
//        reg_r21.bit.DACL_ZC_EN_P = 1;
//        reg_r22.all |= R22_LOLN_EN | R22_LOLP_EN;
//        if (val == 0)
//            reg_r22.all &= ~(R22_LOLN_MUTE | R22_LOLP_MUTE);
//    } else {
//        reg_r17.bit.DACL_DWAEN = 0;
//        reg_r21.bit.DACL_ZC_EN_P = 0;
//        reg_r22.all &= ~(R22_LOLN_EN | R22_LOLP_EN);
//        if (val == 0)
//            reg_r22.all |= R22_LOLN_MUTE | R22_LOLP_MUTE;
//    }
//
//    if (dac->info->ch_bmp & DAC_BMP_RIGHT) {
//        reg_r17.bit.DACR_DWAEN = 1;
//        reg_r21.bit.DACR_ZC_EN_P = 1;
//        reg_r22.all |= R22_LORN_EN | R22_LORP_EN;
//        if (val == 0)
//            reg_r22.all &= ~(R22_LORN_MUTE | R22_LORP_MUTE);
//    } else {
//        reg_r17.bit.DACR_DWAEN = 0;
//        reg_r21.bit.DACR_ZC_EN_P = 0;
//        reg_r22.all &= ~(R22_LORN_EN | R22_LORP_EN);
//        if (val == 0)
//            reg_r22.all |= R22_LORN_MUTE | R22_LORP_MUTE;
//    }

    //NOTE: ONLY 1 DAC on ARCS, dev_bmp CAN be CH_BMP_LEFT ONLY!
    if (dac->info->ch_bmp & DAC_BMP_LEFT) {
        reg_r17.bit.DAC_DWAEN = 1;
        reg_r21.bit.DAC_ZCEN_REG = 1;
        reg_r22.all |= R22_LON_EN | R22_LOP_EN;
        if (val == 0)
            reg_r22.all &= ~(R22_LON_MUTE | R22_LOP_MUTE);
    } else {
        reg_r17.bit.DAC_DWAEN = 0;
        reg_r21.bit.DAC_ZCEN_REG = 0;
        reg_r22.all &= ~(R22_LON_EN | R22_LOP_EN);
        if (val == 0)
            reg_r22.all |= R22_LON_MUTE | R22_LOP_MUTE;
    }

    if (val == 0) { // called only once
        // enable CODEC external power, clock and pads if necessary, i.e. LDO etc.
        ENABLE_CODEC_BASIC();

        // CODEC analog settings
        CONFIG_CODEC_DAC_ANALOG();

        // initialize analog gain as 0 dB
//        reg_r22.all &= ~(R22_LOL_VOL_MASK | R22_LOR_VOL_MASK);
//        reg_r22.all |= R22_LOL_VOL(9) | R22_LOR_VOL(9);
        reg_r22.all &= ~(R22_LO_VOL_MASK);
        reg_r22.all |= R22_LO_VOL(9);

        // initialize digital gain as 0 dB
        uint32_t val_r16;
        val_r16 = dac->reg->REG_AUD_R16_DAC_CTRL1.all;
//        val_r16 &= ~(R16_GAIN_L_MASK | R16_GAIN_R_MASK);
//        val_r16 |= R16_GAIN_L(0xe1) | R16_GAIN_R(0xe1);
        val_r16 &= ~(R16_GAIN_MASK);
        val_r16 |= R16_GAIN(0xe1);
        dac->reg->REG_AUD_R16_DAC_CTRL1.all = val_r16;
    }

    // R17
    dac->reg->REG_AUD_R17_DAC_CTRL2.all = reg_r17.all;

    // R18
    // DAC_DWA_TYPE @ R18 bit[14:12]
    // 000: circling DWA starting address;
    // 001: increment DWA starting at every clock cycles;
    reg_r18.bit.DAC_DWA_TYPE = 0; // default 1
    dac->reg->REG_AUD_R18_DAC_CTRL3.all = reg_r18.all | R18_DAC_SDM_RELEASE | R18_DAC_DITH_NTF_EN;

    // R21
    dac->reg->REG_AUD_R21_DAC_CTRL6.all = reg_r21.all; // | R21_DAC_DCT_RELEASE;

    // R22
    dac->reg->REG_AUD_R22_DAC_CTRL7.all = reg_r22.all;

    // R15
//    dac->reg->REG_AUD_R15_DAC_CTRL0.all = reg_r15.all | R15_DAC_RELEASE | R15_DACR_ZCTO_EN | R15_DACL_ZCTO_EN;
    dac->reg->REG_AUD_R15_DAC_CTRL0.all = reg_r15.all | R15_DAC_RELEASE | R15_DAC_ZCTO_EN;

    // set configured flag if sampling rate is set...
    if ((control & CSK_DAC_SR_Msk) != 0)
        dac->info->flags |= DAC_FLAG_CONFIGURED;

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t DAC_SetVolume(void *dac_grp, ...)
 \brief       Set analog (line out) and/or digital gain of DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   a_gain    Analog (Line out) Gain of Left Channel (DAC0, @ a_gain[15:0])
                        and Right Channel (DAC1, @ a_gain[31:16]),
                        Analog (Line Out) Gain value: 0x0 (-24dB) ~ 0xF (6dB), 2dB each step, default 0x9 (-6dB)
 \param[in]   d_gain    Digital gain of Left Channel (DAC0, @ d_gain[15:0])
                        and Right Channel (DAC1, @ d_gain[31:16]),
                        Digital Gain value: 0x70 (-113dB) ~ 0xFF (30dB), 1dB each step, default 0xe1 (0dB)
 \param[in]   vol_flag  volume flag which indicates which volume items are specified
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/
int32_t
DAC_SetVolume(void *dac_grp, uint32_t a_gain, uint32_t d_gain, uint32_t vol_flag)
{
    uint32_t val;
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    if (dac == NULL) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x",
                __func__, dac_grp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if (!(vol_flag & DAC_BMP_STEREO) && !(vol_flag & (DAC_BMP_STEREO<<2))) {
        CLOGW("%s: invalid parameter, no volume is set: vol_flag=0x%08x",
                __func__, vol_flag);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

/*
    // Line Out (analog) gain
    if (vol_flag & (DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_A_RIGHT)) {
        union AUD_R22_DAC_CTRL7 reg_r22;
        reg_r22.all = dac->reg->REG_AUD_R22_DAC_CTRL7.all;

        if (vol_flag & DAC_VOL_FLAG_A_LEFT) { // Left Line Out (analog)
            val = a_gain & 0xFFFF;
            if (val > R22_LO_VOL_MAX || val < R22_LO_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r22.bit.LOLVOL_P = val;
        }

        if (vol_flag & DAC_VOL_FLAG_A_RIGHT) { // Right Line Out (analog)
            val = (a_gain >> 16) & 0xFFFF;
            if (val > R22_LO_VOL_MAX || val < R22_LO_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r22.bit.LORVOL_P = val;
        }

        dac->reg->REG_AUD_R22_DAC_CTRL7.all = reg_r22.all;
    }

    // Digital gain
    if (vol_flag & (DAC_VOL_FLAG_D_LEFT | DAC_VOL_FLAG_D_RIGHT)) {
        union AUD_R16_DAC_CTRL1 reg_r16;
        reg_r16.all = dac->reg->REG_AUD_R16_DAC_CTRL1.all;

        if (vol_flag & DAC_VOL_FLAG_D_LEFT) { // Left Digital
            val = d_gain & 0xFFFF;
            if (val > R16_GAIN_MAX || val < R16_GAIN_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r16.bit.DAC_GAIN_L = val;
        }

        if (vol_flag & DAC_VOL_FLAG_D_RIGHT) { // Right Digital
            val = (d_gain >> 16) & 0xFFFF;
            if (val > R16_GAIN_MAX || val < R16_GAIN_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r16.bit.DAC_GAIN_R = val;
        }

        dac->reg->REG_AUD_R16_DAC_CTRL1.all = reg_r16.all;
    }
*/

    // Line Out (analog) gain (ONLY LEFT channel on ARCS!)
    if (vol_flag & DAC_VOL_FLAG_A_LEFT) {
        union AUD_R22_DAC_CTRL7 reg_r22;
        reg_r22.all = dac->reg->REG_AUD_R22_DAC_CTRL7.all;

        //if (vol_flag & DAC_VOL_FLAG_A_LEFT) { // Left Line Out (analog)
            val = a_gain & 0xFFFF;
            if (val > R22_LO_VOL_MAX || val < R22_LO_VOL_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r22.bit.LOLVOL_P = val;
        //}

        dac->reg->REG_AUD_R22_DAC_CTRL7.all = reg_r22.all;
    }

    // Digital gain (ONLY LEFT channel on ARCS!)
    if (vol_flag & DAC_VOL_FLAG_D_LEFT) {
        union AUD_R16_DAC_CTRL1 reg_r16;
        reg_r16.all = dac->reg->REG_AUD_R16_DAC_CTRL1.all;

        //if (vol_flag & DAC_VOL_FLAG_D_LEFT) { // Left Digital
            val = d_gain & 0xFFFF;
            if (val > R16_GAIN_MAX || val < R16_GAIN_MIN)
                return CSK_DRIVER_ERROR_PARAMETER;
            reg_r16.bit.DAC_GAIN = val;
        //}

        dac->reg->REG_AUD_R16_DAC_CTRL1.all = reg_r16.all;
    }

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t DAC_SetMute(void *dac_grp, ...)
 \brief       Set mute/unmute value of DAC device group.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[in]   mute_val  Mute/UnMute bit of Left Channel (DAC0, @ mute_val[0])
                        and Right Channel (DAC1, @ mute_val[1]),
 \param[in]   dev_bmp  specify which DAC devices in the device group
                      DAC0 (Left Channel, @bit[0]) and DAC1 (Right Channel, @bit[1]),
                      and dev_bmp = 0x3 indicates both DAC0 & DAC1
 \return      common \ref execution_status and driver specific \ref DAC execution_status
*/
int32_t
DAC_SetMute(void *dac_grp, uint8_t mute_val, uint8_t dev_bmp)
{
    //int32_t ret = CSK_DRIVER_OK;
    uint8_t mute;
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    uint8_t bmp = 0;

    if (dac == NULL || (bmp = dac->info->ch_bmp & dev_bmp) != DAC_BMP_LEFT) {
        CLOGW("%s: invalid parameter, DAC group: 0x%08x, dev_bmp: %d",
                __func__, dac_grp, dev_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(dac->info->flags & DAC_FLAG_CONFIGURED)) {
        LOGD("%s: DAC group %d has NOT been configured!!",
                __func__, dac->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    union AUD_R17_DAC_CTRL2 reg_r17;
    union AUD_R22_DAC_CTRL7 reg_r22;
    reg_r17.all = dac->reg->REG_AUD_R17_DAC_CTRL2.all;
    reg_r22.all = dac->reg->REG_AUD_R22_DAC_CTRL7.all;

/*
    // Left channel mute setting
    if (dev_bmp & DAC_BMP_LEFT) {
        mute = mute_val & DAC_BMP_LEFT;
        reg_r17.bit.DACMU_L = mute; // digital mute/unmute
        if (mute)
            reg_r22.all |= R22_LOLN_MUTE | R22_LOLP_MUTE; // line out mute
        else
            reg_r22.all &= ~(R22_LOLN_MUTE | R22_LOLP_MUTE); // line out unmute
        dac->info->status.bit.l_mute = mute;
    }

    // Right channel mute setting
    if (dev_bmp & DAC_BMP_RIGHT) {
        mute = (mute_val & DAC_BMP_RIGHT) >> 1;
        reg_r17.bit.DACMU_R = mute; // digital mute/unmute
        if (mute)
            reg_r22.all |= R22_LORN_MUTE | R22_LORP_MUTE; // line out mute
        else
            reg_r22.all &= ~(R22_LORN_MUTE | R22_LORP_MUTE); // line out unmute
        dac->info->status.bit.r_mute = mute;
    }
*/

    // Left channel mute setting
    if (dev_bmp & DAC_BMP_LEFT) {
        mute = mute_val & DAC_BMP_LEFT;
        reg_r17.bit.DACMU = mute; // digital mute/unmute
        if (mute)
            reg_r22.all |= R22_LON_MUTE | R22_LOP_MUTE; // line out mute
        else
            reg_r22.all &= ~(R22_LON_MUTE | R22_LOP_MUTE); // line out unmute
        dac->info->status.bit.l_mute = mute;
    }

    dac->reg->REG_AUD_R17_DAC_CTRL2.all = reg_r17.all;
    dac->reg->REG_AUD_R22_DAC_CTRL7.all = reg_r22.all;
    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t DAC_GetStatus(void *dac_grp, ...)
 \brief       Get DAC device group's status.
 \param[in]   dac_grp  Pointer to DAC device group instance
 \param[out]  status  Pointer to CSK_DAC_STATUS buffer
 \return      \ref execution_status
 */
int32_t
DAC_GetStatus(void *dac_grp, CSK_DAC_STATUS *status)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);

    if (dac == NULL || status == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    (*status).all = dac->info->status.all;
    return CSK_DRIVER_OK;
}


// Set a series of EQ coefficient(s)
int32_t DAC_EQ_Set_Coef_Array(void *dac_grp, uint32_t *eqcoefs, uint32_t num)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || dac->dev_idx != 0) // only DAC01 support EQ so far...
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_set_coef_array(dac->apc_dch, eqcoefs, num);
}

// Set some EQ coefficient of specified index
int32_t DAC_EQ_Set_Coef(void *dac_grp, uint32_t index, uint32_t eqcoef)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || dac->dev_idx != 0) // only DAC01 support EQ so far...
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_set_coef(dac->apc_dch, index, eqcoef);
}

// Enable EQ functions
int32_t DAC_EQ_Enable(void *dac_grp, uint32_t stages)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || dac->dev_idx != 0) // only DAC01 support EQ so far...
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_enable(dac->apc_dch, stages);
}

// Disable EQ functions
int32_t DAC_EQ_Disble(void *dac_grp)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || dac->dev_idx != 0) // only DAC01 support EQ so far...
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_disble(dac->apc_dch);
}

// Clear EQ coefficients
int32_t DAC_EQ_Clear(void *dac_grp, uint8_t wait_done)
{
    DAC_GRP *dac = safe_dac_grp(dac_grp);
    if (dac == NULL || dac->dev_idx != 0) // only DAC01 support EQ so far...
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_clear(dac->apc_dch, wait_done);
}


_FAST_FUNC_RO static void
dac_apc_event(uint32_t event_info, uint32_t usr_param)
{
    uint8_t ch_dir, notify = 1;
    uint32_t dac_event_info;

    uint8_t event_type = event_info & 0xFF;
    DAC_GRP *dac = (DAC_GRP *)usr_param;

    APC_CH apc_ch = (event_info >> 8) & 0xFF;
    APC_CH apc_dch = APC_CH_TO_DCH(apc_ch);
    apc_dual_channel_owner(apc_dch, NULL, NULL, &ch_dir);
    dac_event_info = (APC_CH_LR_IDX(apc_ch) << 8) | (apc_dch << 16);

    assert(dac != NULL);

    if (event_type & APC_EVENT_TRANSFER_COMPLETE) {
        if (!ch_dir) { // OUT
//            if (dac->info->ch_mix) // mixed L/R channels
//                dac->info->status.bit.busy &= ~CH_BMP_STEREO;
//            else if (APC_CH_LR_IDX(apc_ch)) // right channel
//                dac->info->status.bit.busy &= ~CH_BMP_RIGHT;
//            else // left channel
//                dac->info->status.bit.busy &= ~CH_BMP_LEFT;
            dac->info->status.bit.busy &= ~CH_BMP_STEREO; // LEFT Only
            dac_event_info |= CSK_DAC_EVENT_SEND_COMPLETE;
        } else { // ECHO
            assert (dac->info->echo_bmp != 0 && apc_dch == dac->dch_echo);
            dac->info->status.bit.ech_busy &= ~CH_BMP_STEREO; // LEFT Only
            dac_event_info |= CSK_DAC_EVENT_ECHO_RX_COMPLETE;
        }
    }

    if (event_type & APC_EVENT_PIPO_DONE) { // Ping/Pong Transfer Done
         if (!ch_dir) { // OUT
            if (event_info & PIPO_PING_XFER_DONE)
                dac_event_info |= CSK_DAC_EVENT_TX_PING_DONE;
            if (event_info & PIPO_PONG_XFER_DONE)
                dac_event_info |= CSK_DAC_EVENT_TX_PONG_DONE;
        } else { // ECHO
            assert (dac->info->echo_bmp != 0 && apc_dch == dac->dch_echo);
            if (event_info & PIPO_PING_XFER_DONE)
                dac_event_info |= CSK_DAC_EVENT_ECHO_RX_PING_DONE;
            if (event_info & PIPO_PONG_XFER_DONE)
                dac_event_info |= CSK_DAC_EVENT_ECHO_RX_PONG_DONE;
        }
    }

    if (event_type & APC_EVENT_TX_FIFO_UNDERRUN) {
        //dac->info->status.bit.busy = 0;
        dac->info->status.bit.tx_undf = 1;
        dac_event_info |= CSK_DAC_EVENT_TX_FIFO_UNDERRUN;
    }

    if (event_type & APC_EVENT_TX_FIFO_EMPTY) {
        dac->info->status.bit.tx_emp = 1;
        dac_event_info |= CSK_DAC_EVENT_TX_FIFO_EMPTY;
    }

    if (event_type & APC_EVENT_DMA_ERROR) {
        //dac->info->status.bit.busy = 0;
        dac_event_info |= CSK_DAC_EVENT_OTHER_ERROR;
    }

    if (event_type & APC_EVENT_RX_FIFO_OVERRUN) {
        if (dac->info->echo_bmp != 0 && apc_dch == dac->dch_echo) {
            dac->info->status.bit.ech_ovf = 1;
            dac_event_info |= CSK_DAC_EVENT_ECHO_RX_FIFO_OVERRUN;
        }
    }

    // notify DAC caller
    if (notify && dac->info->cb_event != NULL) {
        dac->info->cb_event(dac_event_info, dac->info->usr_param);
    }
}
