/*
 * i2s.c
 *
 *
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "arcs_ap.h"

#include "Driver_I2S.h"
#include "i2s.h"
#include "ClockManager.h" // for CRM_GetSrcFreq()

#include "log_print.h"

#define DEBUG_LOG   1 //0 //
#if DEBUG_LOG
#define LOGD(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

#define I2S_IN_USE_PIO     0 //1 //RX FIFO -> RAM, 1: use PIO, 0: use DMA (default)
#define I2S_OUT_USE_PIO    0 //1 //RAM -> TX FIFO, 1: use PIO, 0: use DMA (default)

#define CSK_I2S_DRV_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)
#define XTAL_FREQ()         CRM_GetSrcFreq(CRM_IpSrcXtalClk)

// driver version
static const
CSK_DRIVER_VERSION i2s_driver_version = { CSK_I2S_API_VERSION, CSK_I2S_DRV_VERSION };

//static void i2s_irq_handler(I2S_DEV *i2s);
static void i2s_apc_event(uint32_t event_info, uint32_t usr_param);

//------------------------------------------------------------------------------------------
#if (ARCS_VER > ARCS_B0_SOC)
static CMN_SYSCFG_RegDef *g_sysctrl = IP_SYSCTRL;
#else
static CMN_SYS_RegDef *g_sysctrl = IP_SYSCTRL;
#endif

// I2S0
static I2S_DEV i2s0_dev;
//static void i2s0_irq_handler(void) { i2s_irq_handler(&i2s0_dev); }

static I2S_INFO i2s0_info = { 0 };
static I2S_DEV i2s0_dev = {
        0, { 0 },
        CSK_I2S0,
        //CSK_I2S0_CLK,
        //IRQ_I2S0_VECTOR,
        //i2s0_irq_handler,

        .apc_res = {
          1, 1, // 1 APC in dual_channel, 1 APC out dual_channel
          1, 0, // 1 APC echo dual_channel, reserved
          APC_DCH_I2S0_IN, // APC IN dual_channel
          APC_DCH_I2S0_OUT, // APC OUT dual_channel
          APC_DCH_ECHO0 // APC ECHO (IN) dual_channel
        },
        .ch_cfg = { 0 },
        &i2s0_info
};

// Get I2S0 device instance: I2S0
void* I2S0() { return &i2s0_dev; }

// I2S1
static I2S_DEV i2s1_dev;
//static void i2s1_irq_handler(void) { i2s_irq_handler(&i2s1_dev); }

static I2S_INFO i2s1_info = { 0 };
static I2S_DEV i2s1_dev = {
        1, { 0 },
        CSK_I2S1,
        //CSK_I2S1_CLK,
        //IRQ_I2S1_VECTOR,
        //i2s1_irq_handler,

        .apc_res = {
          1, 1, // 1 APC in dual_channel, 1 APC out dual_channel
          1, 0, // 1 APC echo dual_channel, reserved
          APC_DCH_I2S1_IN, // APC IN dual_channel
          APC_DCH_I2S1_OUT, // APC OUT dual_channel
          APC_DCH_ECHO1 // APC ECHO (IN) dual_channel
        },
        .ch_cfg = { 0 },
        &i2s1_info
};

// Get I2S1 device instance: I2S1
void* I2S1() { return &i2s1_dev; }

//------------------------------------------------------------------------------------------
I2S_DEV * safe_i2s_dev(void *i2s_dev)
{
    // safe check of I2S device parameter
    if (i2s_dev == &i2s0_dev) {
        if (i2s0_dev.reg != CSK_I2S0 || i2s0_dev.dev_idx != 0) {
            //CLOGW("I2S0 device context has been tampered illegally!!\n");
            return NULL;
        }
    } else if (i2s_dev == &i2s1_dev) {
        if (i2s1_dev.reg != CSK_I2S1 || i2s1_dev.dev_idx != 1) {
            //CLOGW("I2S1 device context has been tampered illegally!!\n");
            return NULL;
        }
    } else {
        return NULL;
    }

    return (I2S_DEV *)i2s_dev;
}


/*
static inline void enable_i2s_rx(I2S_DEV *i2s, uint8_t bmp_in) {
    //assert(i2s != NULL);
    //assert(bmp_in != 0);
    i2s->ch_cfg.enbmp_in |= bmp_in;
    //if (i2s->reg->REG_I2S_CFG0.bit.RX_OFF == 1)
    //    i2s->reg->REG_I2S_CFG0.bit.RX_OFF = 0; //FIXME: enable RX
}

static inline void disable_i2s_rx(I2S_DEV *i2s, uint8_t bmp_in) {
    //assert(i2s != NULL);
    //assert(bmp_in != 0);
    i2s->ch_cfg.enbmp_in &= ~bmp_in;
    //if (i2s->ch_cfg.enbmp_in == 0 && i2s->reg->REG_I2S_CFG0.bit.RX_OFF == 0)
    //    i2s->reg->REG_I2S_CFG0.bit.RX_OFF = 1; //FIXME: disable RX
}

static inline void enable_i2s_tx(I2S_DEV *i2s, uint8_t bmp_out) {
    //assert(i2s != NULL);
    //assert(bmp_out != 0);
    i2s->ch_cfg.enbmp_out |= bmp_out;
//    if (i2s->reg->REG_I2S_CFG0.bit.TX_OFF == 1)
//        i2s->reg->REG_I2S_CFG0.bit.TX_OFF = 0; //enable TX
}

static inline void disable_i2s_tx(I2S_DEV *i2s, uint8_t bmp_out) {
    //assert(i2s != NULL);
    //assert(bmp_out != 0);
    i2s->ch_cfg.enbmp_out &= ~bmp_out;
//    if (i2s->ch_cfg.enbmp_out == 0 && i2s->reg->REG_I2S_CFG0.bit.TX_OFF == 0)
//        i2s->reg->REG_I2S_CFG0.bit.TX_OFF = 1; //disable TX
}
*/

static int32_t i2s_enable_rx_channels(I2S_DEV *i2s, uint8_t ch_bmp_in);
//static int32_t i2s_enable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out);
//static int32_t i2s_enable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo);

static int32_t i2s_disable_rx_channels(I2S_DEV *i2s, uint8_t ch_bmp_in);
//static int32_t i2s_disable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out);
//static int32_t i2s_disable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo);

static void i2s_reset(I2S_DEV *i2s);

//------------------------------------------------------------------------------------------


/**
 \fn          CSK_DRIVER_VERSION SPI_GetVersion (void)
 \brief       Get driver version.
 \return      \ref CSK_DRIVER_VERSION
*/
CSK_DRIVER_VERSION
I2S_GetVersion()
{
    return i2s_driver_version;
}


//JUST HERE!! 2024.1.16. SHOULD specify DMA channel(s) for I2S IN/OUT/ECHO!!

static int32_t
//i2s_acquire_apc_dual_channels(I2S_DEV *i2s, uint8_t ch_bmp_in, uint8_t ch_bmp_out, uint8_t ch_bmp_echo, uint8_t init_flag)
//i2s_acquire_apc_dual_channels(I2S_DEV *i2s, uint8_t ch_bmp_in, uint8_t ch_bmp_out, uint8_t ch_bmp_echo)
i2s_acquire_apc_dual_channels(I2S_DEV *i2s, uint32_t dev_bmp_flag, I2S_DMA_CHS *dma_chs_p)
{
    uint8_t ch_bmp_in, ch_bmp_out, ch_bmp_echo;
    uint8_t in_mask, out_mask;
    uint8_t apc_dch_array[4], dma_chs[2];
    APC_DCH apc_dch;
    int32_t ret, cnt, i;

    assert(i2s != NULL);
    in_mask = (1 << (i2s->apc_res.dch_in_cnt * 2)) - 1;
    out_mask = (1 << (i2s->apc_res.dch_out_cnt * 2)) - 1;

    ch_bmp_in = ((dev_bmp_flag & I2S_BMP_FLAG_IN_STEREO) >> I2S_BMP_FLAG_IN_POS) & in_mask;
    ch_bmp_out = ((dev_bmp_flag & I2S_BMP_FLAG_OUT_STEREO) >> I2S_BMP_FLAG_OUT_POS) & out_mask;
    ch_bmp_echo = (dev_bmp_flag & I2S_BMP_FLAG_ECHO_STEREO) >> I2S_BMP_FLAG_ECHO_POS;

    // neither valid IN channels nor valid OUT channels
    if ( ch_bmp_in == 0 && ch_bmp_out == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    cnt = 0;
    memset(apc_dch_array, 0xFF, sizeof(apc_dch_array));

    if (ch_bmp_in != 0) {
        if (dma_chs_p != NULL) {
            dma_chs[0] = dma_chs_p->dma_ch_in_left;
            dma_chs[1] = dma_chs_p->dma_ch_in_right;
        } else { // default setting
            dma_chs[0] = DMA_CH_I2S_RXL_DEF;
            dma_chs[1] = DMA_CH_I2S_RXR_DEF;
        }
        apc_dch = i2s->apc_res.dch_in;
        ret = apc_dual_channel_acquire(apc_dch, APC_INTF_I2S_IN, i2s->dev_idx, dma_chs, i2s_apc_event, (uint32_t)i2s);
        if (ret != CSK_DRIVER_OK)
            goto ERR_EXIT;
        apc_dch_array[cnt] = apc_dch;
        cnt++;
    }

    if (ch_bmp_out != 0) {
        if (dma_chs_p != NULL) {
            dma_chs[0] = dma_chs_p->dma_ch_out_left;
            dma_chs[1] = dma_chs_p->dma_ch_out_right;
        } else { // default setting
            dma_chs[0] = DMA_CH_I2S_TXL_DEF;
            dma_chs[1] = DMA_CH_I2S_TXR_DEF;
        }
        apc_dch = i2s->apc_res.dch_out;
        ret = apc_dual_channel_acquire(apc_dch, APC_INTF_I2S_OUT, i2s->dev_idx, dma_chs, i2s_apc_event, (uint32_t)i2s);
        if (ret != CSK_DRIVER_OK)
            goto ERR_EXIT;
        apc_dch_array[cnt] = apc_dch;
        cnt++;
    } // end if ch_bmp_out

    if (ch_bmp_echo) {
        //assert(i2s->dev_idx == 2 && i2s->apc_res.support_echo);
        if (dma_chs_p != NULL) {
            dma_chs[0] = dma_chs_p->dma_ch_echo_left;
            dma_chs[1] = dma_chs_p->dma_ch_echo_right;
        } else { // default setting
            dma_chs[0] = DMA_CH_ECHO_RXL_DEF;
            dma_chs[1] = DMA_CH_ECHO_RXR_DEF;
        }
        apc_dch = i2s->apc_res.dch_echo;
        ret = apc_dual_channel_acquire(apc_dch, APC_INTF_ECHO, i2s->dev_idx, dma_chs, i2s_apc_event, (uint32_t)i2s);
       if (ret != CSK_DRIVER_OK)
            goto ERR_EXIT;
        apc_dch_array[cnt] = apc_dch;
        cnt++;
    }

    memset(&i2s->ch_cfg, 0, sizeof(I2S_CH_CFG));
    i2s->ch_cfg.chbmp_in = ch_bmp_in;
    i2s->ch_cfg.chbmp_out = ch_bmp_out;
    i2s->ch_cfg.chbmp_echo = ch_bmp_echo;
    i2s->ch_cfg.data_len = 16; // default 16bits

    //TODO: other operations after APC channels are acquired?

    return CSK_DRIVER_OK;

ERR_EXIT:
    for (i=0; i<cnt; i++) {
        apc_dual_channel_release(apc_dch_array[i]);
    }
    return CSK_DRIVER_ERROR;
}

static void
i2s_release_apc_dual_channels(I2S_DEV *i2s)
{
    uint8_t i;
    APC_DCH apc_dch;
    assert(i2s != NULL);

    if (i2s->ch_cfg.chbmp_in != 0) {
        assert ((i2s->ch_cfg.chbmp_in & CH_BMP_STEREO) != 0);
        //apc_dch = i2s->info->dch_in; //REMOVED:
        apc_dch = i2s->apc_res.dch_in;
        apc_dual_channel_release(apc_dch);
    }

    if (i2s->ch_cfg.chbmp_out != 0) {
        assert ((i2s->ch_cfg.chbmp_out & CH_BMP_STEREO) != 0);
        apc_dch = i2s->apc_res.dch_out;
        apc_dual_channel_release(apc_dch);
    }

    if (i2s->ch_cfg.chbmp_echo != 0) {
        assert ((i2s->ch_cfg.chbmp_echo & CH_BMP_STEREO) != 0);
        apc_dch = i2s->apc_res.dch_echo;
        apc_dual_channel_release(apc_dch);
    }

    memset(&i2s->ch_cfg, 0, sizeof(I2S_CH_CFG));
}


/**
 \fn          int32_t I2S_Initialize (void *i2s_dev, CSK_I2S_SignalEvent_t cb_event, uint32_t usr_param)
 \brief       Initialize I2S Interface.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   cb_event  Pointer to \ref CSK_I2S_SignalEvent_t
 \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
 \param[in]   dev_bmp_flag  indicates I2S IN/OUT/ECHO channels used currently and other flags
 \param[in]   dma_chs_p     Pointer to the structure specifying DMA channels.
 \return      \ref execution_status
*/
//int32_t
//I2S_Initialize(void *i2s_dev, CSK_I2S_SignalEvent_t cb_event, uint32_t usr_param,
//                uint8_t ch_bmp_in, uint8_t ch_bmp_out, uint8_t ch_bmp_echo, uint8_t init_flag)

int32_t
I2S_Initialize(void *i2s_dev, CSK_I2S_SignalEvent_t cb_event, uint32_t usr_param,
               uint32_t dev_bmp_flag, I2S_DMA_CHS *dma_chs_p)
{
    int32_t ret;

    g_sysctrl = IP_SYSCTRL;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || dma_chs_p == NULL) {
        CLOGW("%s: invalid I2S device (0x%08x) or NULL DMA_CHS!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

//    if ((ch_bmp_echo != 0) &&
//        (i2s->dev_idx != 2 || ch_bmp_out == 0)) {
//        CLOGW("%s: only I2S2 with OUTPUT supports soft-ECHO channel!", __func__);
//        return CSK_DRIVER_ERROR_PARAMETER;
//    }

    if (i2s->info->flags & I2S_FLAG_INITIALIZED)
        return CSK_I2S_ERROR_INITED_ALREADY;

    // initialize APC
    ret = apc_initialize();
    if (ret != CSK_DRIVER_OK) {
        CLOGW("%s: failed to call apc_initialize()!", __func__);
        return ret;
    }

    // acquire APC dual_channel
    memset(&i2s->ch_cfg, 0, sizeof(I2S_CH_CFG));
    //ret = i2s_acquire_apc_dual_channels(i2s, ch_bmp_in, ch_bmp_out, ch_bmp_echo, init_flag);
    //ret = i2s_acquire_apc_dual_channels(i2s, ch_bmp_in, ch_bmp_out, ch_bmp_echo);
    ret = i2s_acquire_apc_dual_channels(i2s, dev_bmp_flag, dma_chs_p);
    if (ret != CSK_DRIVER_OK) {
        CLOGW("%s: failed to acquire APC channels!", __func__);
        return ret;
    }

    // initialize I2S run-time resources
    memset(i2s->info, 0, sizeof(I2S_INFO));
    i2s->info->cb_event = cb_event;
    i2s->info->usr_param = usr_param;
    i2s->info->status.all = 0U;

    //TODO: other initialization...

    i2s->info->flags = I2S_FLAG_INITIALIZED; // I2S is initialized
    LOGD("%s: I2S %d, version: API = 0x%x, DRV = 0x%x\n",
            __func__, i2s->dev_idx, i2s_driver_version.api, i2s_driver_version.drv);

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t I2S_Uninitialize (void)
 \brief       De-initialize I2S Interface.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \return      \ref execution_status
*/
int32_t
I2S_Uninitialize(void *i2s_dev)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL) {
        LOGD("%s: invalid I2S device (0x%08x)!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((i2s->info->flags & I2S_FLAG_INITIALIZED) == 0)
        return CSK_DRIVER_ERROR;

    // Abort current tranfers if any
    //I2S_Abort_Channels(i2s_dev, i2s->ch_cfg.chbmp_in, i2s->ch_cfg.chbmp_out);

    //TODO: any other clean operations?

    // Power off I2S if Powered on
    if (i2s->info->flags & I2S_FLAG_POWERED)
        I2S_PowerControl(i2s_dev, CSK_POWER_OFF);

    // Release APC channels
    i2s_release_apc_dual_channels(i2s);

    // uninitialize APC
    apc_uninitialize();

    i2s->info->flags = 0U; // I2S is uninitialized
    LOGD("%s is called for I2S %d!\n", __func__, i2s->dev_idx);
    return CSK_DRIVER_OK;
}


static inline void i2s_clk_enable(I2S_DEV *i2s)
{
    assert(i2s != NULL);
    AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 0x1;
}

static inline void i2s_clk_disable(I2S_DEV *i2s)
{
    assert(i2s != NULL);
    //AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 0x0;
}

//static inline void i2s_sw_reset()
//{
//    AP_CFG->REG_SW_RESET.bit.I2S_RESET = 1;
//}

/*
static inline AUDIO_CLK_SRC i2s_clk_src(I2S_DEV *i2s)
{
    assert(i2s != NULL);
    //I2S0 & I2S1 use adc_clk, I2S2 use dac_clk
    switch (i2s->dev_idx) {
    case 0:
    case 1:
        return adc_clk_src();
    case 2:
        return dac_clk_src();
    default:
        return AUDIO_CLK_SRC_XTAL;
    }
}

static inline void i2s_clk_select(I2S_DEV *i2s, AUDIO_CLK_SRC src)
{
    assert(i2s != NULL);
    //I2S0 & I2S1 use adc_clk, I2S2 use dac_clk
    switch (i2s->dev_idx) {
    case 0:
    case 1:
        adc_clk_select(src);
        return;
    case 2:
        dac_clk_select(src);
        return;
    default:
        return;
    }
}
*/


/**
 \fn          int32_t I2S_PowerControl (void *i2s_dev, CSK_POWER_STATE state)
 \brief       Control I2S Interface Power.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   state  Power state
 \return      \ref execution_status
*/
int32_t
I2S_PowerControl(void *i2s_dev, CSK_POWER_STATE state)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL) {
        LOGD("%s: invalid I2S device (0x%08x)!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    if ((i2s->info->flags & I2S_FLAG_INITIALIZED) == 0U)
        return CSK_DRIVER_ERROR;

    uint32_t val;
    switch (state) {
    case CSK_POWER_OFF:
        // disable I2S IRQ
        //disable_IRQ(i2s->irq_num);

        // abort current TX/RX and clean state if configured
        if (i2s->info->flags & I2S_FLAG_CONFIGURED)
            I2S_Abort_Channels(i2s_dev, i2s->ch_cfg.chbmp_in, i2s->ch_cfg.chbmp_out, i2s->ch_cfg.chbmp_echo);

        i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 0; //set to default 0!

        // disable I2S module
        disable_i2s(i2s);

        // disable I2S clock -- the clock is shared with adc or dac
        // FIXME: is it correct to disable the clock?
        i2s_clk_disable(i2s);

        //TODO: other power-off configuration...

        i2s->info->flags &= ~(I2S_FLAG_POWERED | I2S_FLAG_CONFIGURED);
        break;

    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;

    case CSK_POWER_FULL:

        if ((i2s->info->flags & I2S_FLAG_POWERED) != 0U)
            return CSK_DRIVER_OK;

        i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 0; //set to default 0!

        // I2S reset works ONLY IF ENABLED in the default state, and
        // Set RSTN_BYPASS to 1 to make that I2S CAN be reset when disabled!
        i2s->reg->REG_I2S_CFG0.bit.RSTN_BYPASS = 1;

        // disable I2S initially
        disable_i2s(i2s);

        // enable I2S clock here
        i2s_clk_enable(i2s);

        // reset I2S
        i2s_reset(i2s);

        //TODO: clear I2S run-time resources

        //TODO: other power-on configuration...

        i2s->info->flags |= I2S_FLAG_POWERED; // I2S is powered
        //register_ISR(i2s->irq_num , i2s->irq_handler, NULL); // register I2S ISR
        //enable_IRQ(i2s->irq_num); // enable I2S IRQ
        break;

    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}


/**
 \fn          int32_t I2S_Send (void *i2s_dev, const void *data, uint32_t num)
 \brief       Start sending data to I2S out channel.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   ch_no  I2S channel number (data is sent on the channel)
 \param[in]   data  Pointer to buffer with data to be sent
 \param[in]   num   Number of data items to send
 \return      \ref execution_status
*/
int32_t
I2S_Send_Body(void *i2s_dev, const uint32_t *data, uint32_t num,
            uint32_t buf_offset, uint32_t src_gath, uint8_t ch_bmp, uint8_t tx_flag)
{
    int32_t ret;
    uint32_t reg_val;
    APC_DCH apc_dch;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t start_now = tx_flag & I2S_TX_FLAG_START_NOW;
    uint8_t bmp = (i2s->ch_cfg.chbmp_out & ch_bmp);
    if (bmp == 0) {
        LOGD("%s: channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.tx_busy)
        return CSK_DRIVER_ERROR_BUSY;

    if (!start_now) {
        ret = i2s_disable_tx_channels(i2s, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    ret = CSK_DRIVER_ERROR;
    apc_dch = i2s->apc_res.dch_out;
    reg_val = i2s->reg->REG_I2S_CFG0.all;

    uint8_t nsynca = (tx_flag & I2S_TX_FLAG_NSYNCA) ? 1 : 0;
    uint8_t lch = APC_DCH_TO_CH(apc_dch, 0);

    if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_l != nsynca) { // use left channel for both
            apc_channel_set_nsynca(lch, nsynca);
            i2s->info->tx_nsynca_l = nsynca;
        }
        // check tx_mode to select proper action
        switch (i2s->ch_cfg.tx_mode) {
        case TXMODE_STEREO_STEREO:
            // apc_dual_channel_write will check if mixed mode is set
            ret = apc_dual_channel_write(apc_dch, data, num, buf_offset, src_gath);
            break;
        case TXMODE_MONOL_STEREO:
            // apc_channel_write will check if separated mode is set
            ret = apc_channel_write(lch, data, num, buf_offset, src_gath);
            // only APC LEFT channel need be enabled (i2s_enable_tx_channels)
            // even if both outgoing L/R channels (SPEAKER) are utilized.
            bmp = CH_BMP_LEFT;
            break;
        case TXMODE_MONOR_STEREO:
        case TXMODE_MONO_MONO:
        default:
            CLOGE("%s: DON'T support writing both L&R channels for TX_MODE %d!\n",
                    __func__, i2s->ch_cfg.tx_mode);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
    } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_LEFT) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        if (i2s->ch_cfg.tx_mode != TXMODE_STEREO_STEREO) {
            CLOGE("%s: SHOULD write LEFT channel in TXMODE_STEREO_STEREO!\n",
                    __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_l != nsynca) { // use left channel
            apc_channel_set_nsynca(lch, nsynca);
            i2s->info->tx_nsynca_l = nsynca;
        }
        // apc_channel_write will check if separated mode is set
        ret = apc_channel_write(lch, data, num, buf_offset, src_gath);
    } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        if (i2s->ch_cfg.tx_mode != TXMODE_STEREO_STEREO) {
            CLOGE("%s: SHOULD write RIGHT channel in TXMODE_STEREO_STEREO!\n",
                    __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        uint8_t rch = APC_DCH_TO_CH(apc_dch, 1);
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_r != nsynca) { // use right channel
            apc_channel_set_nsynca(rch, nsynca);
            i2s->info->tx_nsynca_r = nsynca;
        }
        // apc_channel_write will check if separated mode is set
        ret = apc_channel_write(rch, data, num, buf_offset, src_gath);
    }

    if (ret != CSK_DRIVER_OK)
        return ret;

    i2s->info->status.bit.tx_busy = 1;
    i2s->info->status.bit.tx_unf = 0;

    if (start_now) {
        ret = i2s_enable_tx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module

    // NO NEED to enable FORCE_ON on FPGA & ASIC if RSTN_BYPASS = 1!!
    // [SUPPLEMENT] FOR Slave TX, EN_FORCE_ON SHOULD NOT be set,
    //              or else an extra word 0 may be sent to master!!
    //#if (IC_BOARD == 0) // enable FORCE_ON on FPGA, but NOT on ASIC?
    //    if(!i2s->info->master)
    //        i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 1;
    //#endif
    }

    return ret;
}

int32_t
I2S_Send(void *i2s_dev, const uint32_t *data, uint32_t num, uint8_t ch_bmp, uint8_t tx_flag)
{
    return I2S_Send_Body(i2s_dev, data, num, 0, 0, ch_bmp, tx_flag);
}


// Send data via I2S interface in the Ping/Pong mode
// [IN & OUT] blk_cnt_p   Number of PIPO_OUT_BLOCK in the array (*blk_cnt_p <= 2 on ARCS)
int32_t
I2S_Send_PiPo(void *i2s_dev, PIPO_OUT_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t ch_bmp, uint8_t tx_flag)
{
    int32_t ret;
    uint8_t bmp = 0;
    //uint8_t full_chk = !(tx_flag & I2S_TX_FLAG_QUICK_CHECK);

    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (bmp = i2s->ch_cfg.chbmp_out & ch_bmp) == 0 ||
        blks == NULL || blk_cnt_p == NULL) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t started = i2s->info->status.bit.tx_busy;
    uint8_t start_now = 0;
    APC_DCH apc_dch = i2s->apc_res.dch_out;

    if (!started) {
        if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
            LOGD("%s: I2S%d has NOT been configured!!",
                    __func__, i2s->dev_idx);
            return CSK_DRIVER_ERROR;
        }

        //if (i2s->info->status.bit.tx_busy)
        //    return CSK_DRIVER_ERROR_BUSY;

        start_now = tx_flag & I2S_TX_FLAG_START_NOW;
        if (!start_now) {
            ret = i2s_disable_tx_channels(i2s, bmp);
            if (ret != CSK_DRIVER_OK)
                return ret;
        }

        uint32_t reg_val = i2s->reg->REG_I2S_CFG0.all;
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }

        uint8_t nsynca = (tx_flag & I2S_TX_FLAG_NSYNCA) ? 1 : 0;
        uint8_t ch_no;
        // set or clear NOT_SYNC_CACHE if necessary
        if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_LEFT) {
            ch_no = APC_DCH_TO_CH(apc_dch, 0);
            if (i2s->info->tx_nsynca_l != nsynca) { // use left channel for both
                apc_channel_set_nsynca(ch_no, nsynca);
                i2s->info->tx_nsynca_l = nsynca;
            }
        } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT) {
            ch_no = APC_DCH_TO_CH(apc_dch, 1);
            if (i2s->info->tx_nsynca_r != nsynca) { // use right channel
                apc_channel_set_nsynca(ch_no, nsynca);
                i2s->info->tx_nsynca_r = nsynca;
            }
        }
    } // !started

    ret = apc_dch_write_pipo(apc_dch, ch_bmp,  blks, blk_cnt_p, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        i2s->info->status.bit.tx_busy = 1;
        i2s->info->status.bit.tx_unf = 0;

        if (start_now) {
            ret = i2s_enable_tx_channels(i2s, bmp);
            enable_i2s(i2s); // enable I2S module

        // NO NEED to enable FORCE_ON on FPGA & ASIC if RSTN_BYPASS = 1!!
        // [SUPPLEMENT] FOR Slave TX, EN_FORCE_ON SHOULD NOT be set,
        //              or else an extra word 0 may be sent to master!!
        //#if (IC_BOARD == 0) // enable FORCE_ON on FPGA, but NOT on ASIC?
        //    if(!i2s->info->master)
        //        i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 1;
        //#endif
        }
    }

    return ret;
}


//[OUT]  blks  Pointer to array of PIPO_OUT_BLOCK to hold transferred block descriptors
//[IN]   blk_cnt  Number of PIPO_OUT_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
I2S_PiPo_Txed_Blocks(void *i2s_dev, PIPO_OUT_BLOCK *blks, uint8_t blk_cnt, uint8_t ch_bmp)
{
    int32_t ret;
    uint8_t bmp;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);

    if (i2s == NULL || (bmp = i2s->ch_cfg.chbmp_out & ch_bmp) == 0 ||
        blks == NULL || blk_cnt == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED) || !i2s->info->status.bit.tx_busy) {
        CLOGW("%s: I2S has NOT been configured or started (TX)!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    return apc_dch_get_pipo_blks(i2s->apc_res.dch_out, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
}


//NOTE: _LLP API functions (Multi-block transfer) is NOT supported
//      since ARCS C0, for no LLP support on GPDMAC...

/*
//NOTE: AUDIO_BUFFER_USER array should be KEPT until transfer is completed or aborted!
int32_t
I2S_Send_LLP_Body(void *i2s_dev, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt,
                 uint32_t buf_offset, uint32_t src_gath,
                 uint8_t ch_bmp, uint8_t tx_flag)
{
    int32_t ret;
    uint32_t reg_val;
    APC_DCH apc_dch;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t start_now = tx_flag & I2S_TX_FLAG_START_NOW;
    uint8_t bmp = (i2s->ch_cfg.chbmp_out & ch_bmp);
    if (bmp == 0) {
        LOGD("%s: channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!", __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.tx_busy)
        return CSK_DRIVER_ERROR_BUSY;

    if (!start_now) {
        ret = i2s_disable_tx_channels(i2s, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    ret = CSK_DRIVER_ERROR;
    apc_dch = i2s->apc_res.dch_out;
    reg_val = i2s->reg->REG_I2S_CFG0.all;

    uint8_t nsynca = (tx_flag & I2S_TX_FLAG_NSYNCA) ? 1 : 0;
    uint8_t lch = APC_DCH_TO_CH(apc_dch, 0);

    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_l != nsynca) { // use left channel for both
            apc_channel_set_nsynca(lch, nsynca);
            i2s->info->tx_nsynca_l = nsynca;
        }
        // check tx_mode to select proper action
        switch (i2s->ch_cfg.tx_mode) {
        case TXMODE_STEREO_STEREO:
            // apc_dual_channel_write_LLP will check if mixed mode is set
            ret = apc_dual_channel_write_LLP(apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
            break;
        case TXMODE_MONOL_STEREO:
            // apc_channel_write will check if separated mode is set
            ret = apc_channel_write_LLP(lch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
            // only APC LEFT channel need be enabled (i2s_enable_tx_channels)
            // even if both outgoing L/R channels (SPEAKER) are utilized.
            bmp = CH_BMP_LEFT;
            break;
        case TXMODE_MONOR_STEREO:
        case TXMODE_MONO_MONO:
        default:
            CLOGE("%s: DON'T support writing both L&R channels for TX_MODE %d!\n",
                    __func__, i2s->ch_cfg.tx_mode);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        if (i2s->ch_cfg.tx_mode != TXMODE_STEREO_STEREO) {
            CLOGE("%s: SHOULD write LEFT channel in TXMODE_STEREO_STEREO!\n",
                    __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_l != nsynca) { // use left channel
            apc_channel_set_nsynca(lch, nsynca);
            i2s->info->tx_nsynca_l = nsynca;
        }
        // apc_channel_write_LLP will check if separated mode is set
        ret = apc_channel_write_LLP(lch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);

    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT) {
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
        if (i2s->ch_cfg.tx_mode != TXMODE_STEREO_STEREO) {
            CLOGE("%s: SHOULD write RIGHT channel in TXMODE_STEREO_STEREO!\n",
                    __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        uint8_t rch = APC_DCH_TO_CH(apc_dch, 1);
        // set or clear NOT_SYNC_CACHE if necessary
        if (i2s->info->tx_nsynca_r != nsynca) { // use right channel
            apc_channel_set_nsynca(rch, nsynca);
            i2s->info->tx_nsynca_r = nsynca;
        }

        // apc_channel_write will_LLP check if separated mode is set
        ret = apc_channel_write_LLP(rch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, buf_offset, src_gath);
    }

    if (ret != CSK_DRIVER_OK)
        return ret;

    i2s->info->status.bit.tx_busy = 1;
    i2s->info->status.bit.tx_unf = 0;

    if (start_now) {
//        enable_i2s(i2s); // enable I2S module
        ret = i2s_enable_tx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module
    }

    return ret;
}

int32_t
I2S_Send_LLP(void *i2s_dev, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t ch_bmp, uint8_t tx_flag)
{
    return I2S_Send_LLP_Body(i2s_dev, bufs, buf_cnt, 0, 0, ch_bmp, tx_flag);
}
*/


/**
 \fn          int32_t I2S_Receive (void *i2s_dev, void *data, uint32_t num)
 \brief       Start receiving data from I2S in channel.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   ch_no  I2S channel number (data is received on the channel)
 \param[out]  data  Pointer to buffer for data to receive from I2S in channel
 \param[in]   num   Number of data items to receive
 \return      \ref execution_status
*/
int32_t
I2S_Receive(void *i2s_dev, uint32_t *data, uint32_t num, uint8_t ch_bmp, uint8_t rx_flag)
{
    int32_t ret;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t start_now = rx_flag & I2S_RX_FLAG_START_NOW;
    uint8_t bmp = (i2s->ch_cfg.chbmp_in & ch_bmp);
    if (bmp == 0) {
        LOGD("%s: channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.rx_busy)
        return CSK_DRIVER_ERROR_BUSY;

    if (!start_now) {
        ret = i2s_disable_rx_channels(i2s, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    ret = CSK_DRIVER_ERROR;
    //APC_DCH apc_dch = i2s->info->dch_in;
    APC_DCH apc_dch = i2s->apc_res.dch_in;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read(apc_dch, data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 0), data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 1), data, num, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    i2s->info->status.bit.rx_busy = 1;
    i2s->info->status.bit.rx_ovf = 0;

    if (start_now) {
        ret = i2s_enable_rx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module

    // NO NEED to enable FORCE_ON on FPGA & ASIC if RSTN_BYPASS = 1!!
    // [SUPPLEMENT] FOR Slave RX, EN_FORCE_ON SHOULD be set,
    //              or else the first data from master may be lost!!
    //#if (IC_BOARD == 0) // enable FORCE_ON on FPGA, but NOT on ASIC?
        if(!i2s->info->master)
            i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 1;
    //#endif
    }

    return ret;
}


/*
int32_t
I2S_Receive_LLP(void *i2s_dev, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t ch_bmp, uint8_t rx_flag)
{
    int32_t ret;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t start_now = rx_flag & I2S_RX_FLAG_START_NOW;
    uint8_t bmp = (i2s->ch_cfg.chbmp_in & ch_bmp);
    if (bmp == 0) {
        LOGD("%s: channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.rx_busy)
        return CSK_DRIVER_ERROR_BUSY;

    if (!start_now) {
        ret = i2s_disable_rx_channels(i2s, bmp);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    ret = CSK_DRIVER_ERROR;
    //APC_DCH apc_dch = i2s->info->dch_in;
    APC_DCH apc_dch = i2s->apc_res.dch_in;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read_LLP(apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 0), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 1), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    i2s->info->status.bit.rx_busy = 1;
    i2s->info->status.bit.rx_ovf = 0;

    if (start_now) {
        ret = i2s_enable_rx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module
    }

    return ret;
}
*/


int32_t i2s_enable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo)
{
    uint8_t bmp;
    int32_t ret = CSK_DRIVER_ERROR;

    assert(i2s != NULL);
    bmp = (ch_bmp_echo & i2s->ch_cfg.chbmp_echo);
    if (bmp != 0) { // ECHO channels
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret = apc_dual_channel_enable(i2s->apc_res.dch_echo);
        } else if (bmp & CH_BMP_LEFT) {
            ret = apc_channel_enable(APC_DCH_TO_CH(i2s->apc_res.dch_echo, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret = apc_channel_enable(APC_DCH_TO_CH(i2s->apc_res.dch_echo, 1));
        } else {
            assert(0);
        }
    }

    return ret;
}

int32_t i2s_disable_echo_channels(I2S_DEV *i2s, uint8_t ch_bmp_echo)
{
    uint8_t bmp;
    int32_t ret = CSK_DRIVER_ERROR;

    assert(i2s != NULL);
    bmp = (ch_bmp_echo & i2s->ch_cfg.chbmp_echo);
    if (bmp != 0) { // ECHO channels
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret = apc_dual_channel_disable(i2s->apc_res.dch_echo);
        } else if (bmp & CH_BMP_LEFT) {
            ret = apc_channel_disable(APC_DCH_TO_CH(i2s->apc_res.dch_echo, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret = apc_channel_disable(APC_DCH_TO_CH(i2s->apc_res.dch_echo, 1));
        } else {
            assert(0);
        }
    }

    return ret;
}

int32_t
I2S_Echo_Receive(void *i2s_dev, uint32_t *data, uint32_t num, uint8_t ch_bmp)
{
    int32_t ret;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, ECHO channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t bmp = (i2s->ch_cfg.chbmp_echo & ch_bmp);
    if (!i2s->apc_res.support_echo || bmp == 0) {
        LOGD("%s: softECHO is NOT supported for I2S%d, or echo channel_bmp 0x%x is NOT configured!!",
                __func__, i2s->dev_idx, ch_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.ech_busy)
        return CSK_DRIVER_ERROR_BUSY;

    ret = CSK_DRIVER_ERROR;
    APC_DCH apc_dch = i2s->apc_res.dch_echo;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read(apc_dch, data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 0), data, num, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read(APC_DCH_TO_CH(apc_dch, 1), data, num, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    ret = i2s_enable_echo_channels(i2s, bmp);

    //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
    if (ret == CSK_DRIVER_OK) {
        i2s->info->status.bit.ech_busy = 1;
        i2s->info->status.bit.ech_ovf = 0;
        ret = i2s_enable_tx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module
    }

    return ret;
}


/*
int32_t
I2S_Echo_Receive_LLP(void *i2s_dev, AUDIO_BUFFER_USER *bufs, uint32_t buf_cnt, uint8_t ch_bmp)
{
    int32_t ret;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t bmp = (i2s->ch_cfg.chbmp_in & ch_bmp);
    if (!i2s->apc_res.support_echo || bmp == 0) {
        LOGD("%s: softECHO is NOT supported for I2S%d, or echo channel_bmp 0x%x is NOT configured!!",
                __func__, i2s->dev_idx, ch_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->status.bit.ech_busy)
        return CSK_DRIVER_ERROR_BUSY;

    ret = CSK_DRIVER_ERROR;
    APC_DCH apc_dch = i2s->apc_res.dch_echo;
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO)
        ret = apc_dual_channel_read_LLP(apc_dch, (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 0), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        ret = apc_channel_read_LLP(APC_DCH_TO_CH(apc_dch, 1), (AUDIO_BUFFER_LLI *)bufs, buf_cnt, 0, 0);

    if (ret != CSK_DRIVER_OK)
        return ret;

    ret = i2s_enable_echo_channels(i2s, bmp);

    //FIXME: SHOULD enable APC TX channels simultaneously if ECHO is required
    if (ret == CSK_DRIVER_OK) {
        i2s->info->status.bit.ech_busy = 1;
        i2s->info->status.bit.ech_ovf = 0;
        ret = i2s_enable_tx_channels(i2s, bmp);
        enable_i2s(i2s); // enable I2S module
    }

    return ret;
}
*/


// Receive data via I2S interface in the Ping/Pong mode
int32_t
I2S_Receive_PiPo(void *i2s_dev, PIPO_IN_BLOCK *blks, uint8_t *blk_cnt_p,
              uint8_t ch_bmp, uint8_t rx_flag)
{
    int32_t ret;
    uint8_t bmp = 0;
    //uint8_t full_chk = !(rx_flag & I2S_RX_FLAG_QUICK_CHECK);

    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (bmp = i2s->ch_cfg.chbmp_in & ch_bmp) == 0 ||
        blks == NULL || blk_cnt_p == NULL) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t started = i2s->info->status.bit.rx_busy;
    uint8_t start_now = 0;
    APC_DCH apc_dch = i2s->apc_res.dch_in;

    if (!started) {
        if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
            LOGD("%s: I2S%d has NOT been configured!!",
                    __func__, i2s->dev_idx);
            return CSK_DRIVER_ERROR;
        }

        //if (i2s->info->status.bit.rx_busy)
        //    return CSK_DRIVER_ERROR_BUSY;

        start_now = rx_flag & I2S_RX_FLAG_START_NOW;
        if (!start_now) {
            ret = i2s_disable_rx_channels(i2s, bmp);
            if (ret != CSK_DRIVER_OK)
                return ret;
        }

        uint32_t reg_val = i2s->reg->REG_I2S_CFG0.all;
        // set to 0, NOT SWAP L/R channel
        if (reg_val & BIT_MASK_SWAP_CHLR_OUT) {
            reg_val &= ~BIT_MASK_SWAP_CHLR_OUT;
            i2s->reg->REG_I2S_CFG0.all = reg_val;
        }
    } // !started

    ret = apc_dch_read_pipo(apc_dch, ch_bmp,  blks, blk_cnt_p, 0, 0);
    if (ret != CSK_DRIVER_OK)
        return ret;

    if (!started) { // not started
        i2s->info->status.bit.rx_busy = 1;
        i2s->info->status.bit.rx_ovf = 0;

        if (start_now) {
            ret = i2s_enable_rx_channels(i2s, bmp);
            enable_i2s(i2s); // enable I2S module

            // NO NEED to enable FORCE_ON on FPGA & ASIC if RSTN_BYPASS = 1!!
            // [SUPPLEMENT] FOR Slave RX, EN_FORCE_ON SHOULD be set,
            //              or else the first data from master may be lost!!
            //#if (IC_BOARD == 0) // enable FORCE_ON on FPGA, but NOT on ASIC?
                if(!i2s->info->master)
                    i2s->reg->REG_I2S_CFG0.bit.EN_FORCE_ON = 1;
            //#endif
        }
    }

    return ret;
}


//[OUT]  blks  Pointer to array of PIPO_IN_BLOCK to hold received block descriptors
//[IN]   blk_cnt  Number of PIPO_IN_BLOCK in the array
// return count of transferred block if >= 0, else return the error value.
int32_t
I2S_PiPo_Rxed_Blocks(void *i2s_dev, PIPO_IN_BLOCK *blks, uint8_t blk_cnt, uint8_t ch_bmp)
{
    int32_t ret;
    uint8_t bmp;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);

    if (i2s == NULL || (bmp = i2s->ch_cfg.chbmp_in & ch_bmp) == 0 ||
        blks == NULL || blk_cnt == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED) || !i2s->info->status.bit.rx_busy) {
        CLOGW("%s: I2S has NOT been configured or started (RX)!!", __func__);
        return CSK_DRIVER_ERROR;
    }

    return apc_dch_get_pipo_blks(i2s->apc_res.dch_in, bmp, (PIPO_IO_BLOCK *)blks, blk_cnt);
}


/**
 \fn          int32_t I2S_Abort_Channels (void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out)
 \brief       Abort data transfer of I2S in and/or out channels if any.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   ch_bmp_in  I2S in channel bitmap (bit0 for I2S_CH_IN0, bit1 for I2S_CH_IN1, ...)
 \param[in]   ch_bmp_out  I2S out channel bitmap (bit0 for I2S_CH_OUT0, bit1 for I2S_CH_OUT1, ...)
 \return      \ref execution_status
*/
int32_t
I2S_Abort_Channels(void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out, uint8_t ch_bmp_echo)
{
    uint8_t bmp;
    APC_DCH apc_dch;
    uint32_t ret = CSK_DRIVER_OK;
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp_in == 0 && ch_bmp_out == 0 && ch_bmp_echo == 0)) {
        LOGD("%s: invalid parameter: I2S device (0x%08x), ch_bmp_in = 0x%x, ch_bmp_out = 0x%x, ch_bmp_echo = 0x%x!",
                __func__, i2s_dev, ch_bmp_in, ch_bmp_out, ch_bmp_echo);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((ch_bmp_in & i2s->ch_cfg.chbmp_in & CH_BMP_STEREO) != 0) { // IN channels
        //apc_dch = i2s->info->dch_in;
        apc_dch = i2s->apc_res.dch_in;

        ret = apc_dual_channel_abort(apc_dch);
        if (ret != CSK_DRIVER_OK)
            goto MY_EXIT;

        i2s->info->status.bit.rx_busy = 0;
        //TODO: abort i2s-specific transfer on IN both L/R channels...
    }

    if ((ch_bmp_out & i2s->ch_cfg.chbmp_out & CH_BMP_STEREO) != 0) { // OUT channels
        apc_dch = i2s->apc_res.dch_out;
        ret = apc_dual_channel_abort(apc_dch);
        if (ret != CSK_DRIVER_OK)
            goto MY_EXIT;

        i2s->info->status.bit.tx_busy = 0;
        //TODO: abort i2s-specific transfer on OUT both L/R channels...
    }

    if ((ch_bmp_echo & i2s->ch_cfg.chbmp_echo & CH_BMP_STEREO) != 0) { // ECHO channels
        apc_dch = i2s->apc_res.dch_echo;
        ret = apc_dual_channel_abort(apc_dch);
        if (ret != CSK_DRIVER_OK)
            goto MY_EXIT;

        i2s->info->status.bit.ech_busy = 0;
        //TODO: abort i2s-specific transfer on ECHO both L/R channels...
    }

    //BSD: just disable I2S module if no RX, TX, ECHO RX operation
    //!status.bit.tx_busy && !status.bit.rx_busy && !status.bit.ech_busy
    if ((i2s->info->status.all & CSK_I2S_STATUS_BUSY_MASK) == 0)
        disable_i2s(i2s);

MY_EXIT:
//    if (ch_bmp_in != 0) {
//        bmp = i2s->ch_cfg.chbmp_in;
//        bmp = ch_bmp_in & ~bmp;
//        if ( bmp != 0) {
//            LOGD("%s: There are invalid IN I2S%d channels (bmp=0x%x)!!",
//                    __func__, i2s->dev_idx, bmp);
//        }
//    }
//    if (ch_bmp_out != 0) {
//        bmp = i2s->ch_cfg.chbmp_out;
//        bmp = ch_bmp_in & ~bmp;
//        if ( bmp != 0) {
//            LOGD("%s: There are invalid OUT I2S%d channels (bmp=0x%x)!!",
//                    __func__, i2s->dev_idx, bmp);
//        }
//    }

    return ret;
}


int32_t i2s_enable_rx_channels(I2S_DEV *i2s, uint8_t ch_bmp_in)
{
    uint8_t bmp;
    APC_DCH apc_dch;
    int32_t ret0 = CSK_DRIVER_OK;

    assert(i2s != NULL);

    bmp = (ch_bmp_in & i2s->ch_cfg.chbmp_in);
    if (bmp != 0) { // IN channels
        ret0 = CSK_DRIVER_ERROR;
        //apc_dch = i2s->info->dch_in;
        apc_dch = i2s->apc_res.dch_in;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret0 = apc_dual_channel_enable(apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            ret0 = apc_channel_enable(APC_DCH_TO_CH(apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret0 = apc_channel_enable(APC_DCH_TO_CH(apc_dch, 1));
        } else {
            assert(0);
        }

//        if (ret0 == CSK_DRIVER_OK)
//            enable_i2s_rx(i2s, bmp); //enable I2S RX
    }

//    if (ch_bmp_in != 0) {
//        bmp = i2s->ch_cfg.chbmp_in;
//        bmp = ch_bmp_in & ~bmp;
//        if ( bmp != 0) {
//            LOGD("%s: There are invalid IN I2S%d channels (bmp=0x%x)!!",
//                    __func__, i2s->dev_idx, bmp);
//        }
//    }

    return ret0;
}

int32_t i2s_enable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out)
{
    uint8_t bmp;
    APC_DCH apc_dch;
    int32_t ret = CSK_DRIVER_OK;

/*  //REMOVED:
    // if OUT SRC (SRC48K) is enabled, start it here...
    if (apc_out_src_is_enabled())
        apc_out_src_start(true);
*/

    assert(i2s != NULL);
    bmp = (ch_bmp_out & i2s->ch_cfg.chbmp_out);
    if (bmp != 0) { // OUT channels
        ret = CSK_DRIVER_ERROR;
        apc_dch = i2s->apc_res.dch_out;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            // enable OUT both L/R channels...
            ret = apc_dual_channel_enable(apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            // enable OUT Left channel...
            ret = apc_channel_enable(APC_DCH_TO_CH(apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            // enable OUT Right channel...
            ret = apc_channel_enable(APC_DCH_TO_CH(apc_dch, 1));
        } else {
            assert(0);
        }

//        if (ret == CSK_DRIVER_OK)
//            enable_i2s_tx(i2s, bmp); //enable I2S TX
    }

//    if (ch_bmp_out != 0) {
//        bmp = i2s->ch_cfg.chbmp_out;
//        bmp = ch_bmp_out & ~bmp;
//        if ( bmp != 0) {
//            LOGD("%s: There are invalid OUT I2S%d channels (bmp=0x%x)!!",
//                    __func__, i2s->dev_idx, bmp);
//        }
//    }

    return ret;
}

/**
 \fn          int32_t I2S_Enable_Channels (void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out)
 \brief       Enable I2S in and/or out channels, and data are started to transfer on the lines
              from now on if I2s_Receive_XXX and/or I2s_Send_XXX are called before.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   ch_bmp_in  I2S in channel bitmap (bit0 for I2S_CH_IN0, bit1 for I2S_CH_IN1, ...)
 \param[in]   ch_bmp_out  I2S out channel bitmap (bit0 for I2S_CH_OUT0, bit1 for I2S_CH_OUT1, ...)
 \return      \ref execution_status
*/
int32_t
I2S_Enable_Channels(void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL) {
        LOGD("%s: invalid I2S device (0x%08x)!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    int32_t ret0 = CSK_DRIVER_OK, ret = CSK_DRIVER_OK;
    if (ch_bmp_in != 0)
        ret0 = i2s_enable_rx_channels(i2s, ch_bmp_in);
    if (ch_bmp_out != 0)
        ret = i2s_enable_tx_channels(i2s, ch_bmp_out);

    if (ret0 == CSK_DRIVER_OK || ret == CSK_DRIVER_OK)
        enable_i2s(i2s); // enable I2S module

    if (ret0 != CSK_DRIVER_OK)
        ret = ret0;

    return ret;
}


int32_t i2s_disable_rx_channels(I2S_DEV *i2s, uint8_t ch_bmp_in)
{
    uint8_t bmp;
    APC_DCH apc_dch;
    int32_t ret0 = CSK_DRIVER_OK;

    assert(i2s != NULL);
    bmp = (ch_bmp_in & i2s->ch_cfg.chbmp_in);
    if (bmp != 0) { // IN channels
        ret0 = CSK_DRIVER_ERROR;
        //apc_dch = i2s->info->dch_in;
        apc_dch = i2s->apc_res.dch_in;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret0 = apc_dual_channel_disable(apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            ret0 = apc_channel_disable(APC_DCH_TO_CH(apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret0 = apc_channel_disable(APC_DCH_TO_CH(apc_dch, 1));
        }

//        if (ret0 == CSK_DRIVER_OK)
//            disable_i2s_rx(i2s, bmp); //disable I2S RX
    }

    if (ch_bmp_in != 0) {
        bmp = i2s->ch_cfg.chbmp_in;
        bmp = ch_bmp_in & ~bmp;
        if ( bmp != 0) {
            LOGD("%s: There are invalid IN I2S%d channels (bmp=0x%x)!!",
                    __func__, i2s->dev_idx, bmp);
        }
    }

    return ret0;
}

int32_t i2s_disable_tx_channels(I2S_DEV *i2s, uint8_t ch_bmp_out)
{
    uint8_t bmp;
    APC_DCH apc_dch;
    int32_t ret = CSK_DRIVER_OK;

    assert(i2s != NULL);
    bmp = (ch_bmp_out & i2s->ch_cfg.chbmp_out);
    if (bmp != 0) { // OUT channels
        ret = CSK_DRIVER_ERROR;
        apc_dch = i2s->apc_res.dch_out;
        if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
            ret = apc_dual_channel_disable(apc_dch);
        } else if (bmp & CH_BMP_LEFT) {
            ret = apc_channel_disable(APC_DCH_TO_CH(apc_dch, 0));
        } else if (bmp & CH_BMP_RIGHT) {
            ret = apc_channel_disable(APC_DCH_TO_CH(apc_dch, 1));
        }

//        if (ret == CSK_DRIVER_OK)
//            disable_i2s_tx(i2s, bmp); //disable I2S TX
    }

    if (ch_bmp_out != 0) {
        bmp = i2s->ch_cfg.chbmp_out;
        bmp = ch_bmp_out & ~bmp;
        if ( bmp != 0) {
            LOGD("%s: There are invalid OUT I2S%d channels (bmp=0x%x)!!",
                    __func__, i2s->dev_idx, bmp);
        }
    }

    return ret;
}

/**
 \fn          int32_t I2S_Disable_Channels (void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out)
 \brief       Disable I2S in and/or out channels (and data transfer on the lines are suspended if any).
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   ch_bmp_in  I2S in channel bitmap (bit0 for I2S_CH_IN0, bit1 for I2S_CH_IN1, ...)
 \param[in]   ch_bmp_out  I2S out channel bitmap (bit0 for I2S_CH_OUT0, bit1 for I2S_CH_OUT1, ...)
 \return      \ref execution_status
*/
int32_t
I2S_Disable_Channels(void *i2s_dev, uint8_t ch_bmp_in, uint8_t ch_bmp_out)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL) {
        LOGD("%s: invalid I2S device (0x%08x)!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    int32_t ret0 = CSK_DRIVER_OK, ret = CSK_DRIVER_OK;
    if (ch_bmp_in != 0)
        ret0 = i2s_disable_rx_channels(i2s, ch_bmp_in);
    if (ch_bmp_out != 0)
        ret = i2s_disable_tx_channels(i2s, ch_bmp_out);

    //if (i2s->ch_cfg.enbmp_in == 0 && i2s->ch_cfg.enbmp_out == 0)
    //    disable_i2s(i2s); // disable I2S module

    if (ret0 != CSK_DRIVER_OK)
        ret = ret0;

    return ret;
}


/**
 \fn          uint32_t I2S_GetTxCount (void *i2s_dev)
 \brief       Get transferred data count.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \return      number of data items transferred if positive,
              error value if negative.
*/
int32_t
I2S_GetTxCount(void *i2s_dev, uint8_t ch_bmp)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((i2s->ch_cfg.chbmp_out & ch_bmp) == 0) {
        LOGD("%s: OUT channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH apc_dch = i2s->apc_res.dch_out;
    if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (i2s->ch_cfg.mix_out)
            return apc_dual_channel_get_count(apc_dch);

        int32_t ret = 0;
        if (i2s->ch_cfg.chbmp_out & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
        if (i2s->ch_cfg.chbmp_out & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));
        return ret;

    } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
    else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));

    return CSK_DRIVER_ERROR;
}

/**
 \fn          uint32_t I2S_GetRxCount (void *i2s_dev)
 \brief       Get transferred data count.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \return      number of data items transferred if positive,
              error value if negative.
*/
int32_t
I2S_GetRxCount(void *i2s_dev, uint8_t ch_bmp)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((i2s->ch_cfg.chbmp_in & ch_bmp) == 0) {
        LOGD("%s: IN channel_bmp 0x%x for I2S%d is NOT configured!!",
                __func__, ch_bmp, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    //APC_DCH apc_dch = i2s->info->dch_in;
    APC_DCH apc_dch = i2s->apc_res.dch_in;
    if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (i2s->ch_cfg.mix_in)
            return apc_dual_channel_get_count(apc_dch);

        int32_t ret = 0;
        if (i2s->ch_cfg.chbmp_in & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
        if (i2s->ch_cfg.chbmp_in & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));
        return ret;

    } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
    else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));

    return CSK_DRIVER_ERROR;
}


/**
 \fn          uint32_t I2S_GetEchoCount (void *i2s_dev)
 \brief       Get transferred echo data count.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \return      number of data items transferred if positive,
              error value if negative.
*/
int32_t
I2S_GetEchoCount(void *i2s_dev, uint8_t ch_bmp)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL || (ch_bmp & CH_BMP_STEREO) == 0) {
        LOGD("%s: invalid parameter, I2S device: 0x%08x, channel_bmp: %d",
                __func__, i2s_dev, ch_bmp);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ( !i2s->apc_res.support_echo || (i2s->ch_cfg.chbmp_echo & ch_bmp) == 0) {
        LOGD("%s: soft ECHO for I2S%d is NOT supported, or channel_bmp 0x%x is NOT configured!!",
                __func__, i2s->dev_idx, ch_bmp);
        return CSK_DRIVER_ERROR;
    }

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!", __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH apc_dch = i2s->apc_res.dch_echo;
    if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_STEREO) {
        if (i2s->ch_cfg.mix_echo)
            return apc_dual_channel_get_count(apc_dch);

        int32_t ret = 0;
        if (i2s->ch_cfg.chbmp_echo & CH_BMP_LEFT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
        if (i2s->ch_cfg.chbmp_echo & CH_BMP_RIGHT)
            ret += apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));
        return ret;

    } else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 0));
    else if ((ch_bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(apc_dch, 1));

    return CSK_DRIVER_ERROR;
}


#define BCK_LRCK_BITLEN     5
#define SLOT_LRCK_BITLEN    7
#define BCK_DIV_BITLEN      10
#define BCK_LRCK_MASK       ((1 << BCK_LRCK_BITLEN) - 1)
#define SLOT_LRCK_MASK      ((1 << SLOT_LRCK_BITLEN) - 1)
#define BCK_DIV_MASK        ((1 << BCK_DIV_BITLEN) - 1)
#define BCK_DIV_MAX         BCK_DIV_MASK

//#if (IC_BOARD == 0) // FPGA, 25bits for 24bits_H/L may not work for SLAVE RX on FPGA platform...
//static const uint32_t bck_factors[] = { 30, 50, 75, 100, 150, 200, 250, 300, 500 };
//#define BCKF_CNT_FOR_NON_TDM    2
//#else // ASIC, 30bits may work with least padding bits for MCLK=24MHz, SR=16KHz
static const uint32_t bck_factors[] = { 25, 30, 50, 75, 100, 150, 200, 250, 300, 500 };
#define BCKF_CNT_FOR_NON_TDM    3
//#endif

#define BCKF_CNT    (sizeof(bck_factors)/sizeof(bck_factors[0]))
#define BCK_MARGIN  1 // 2 // 5

// meaningful only in I2S master mode
static bool i2s_set_samp_rate(I2S_DEV *i2s, uint32_t samp_rate, I2S_CFG0 *cfg0_p, I2S_CFG1 *cfg1_p)
{
    uint32_t i, val, val2, clk_in, div;
    assert(i2s != NULL && samp_rate != 0 && cfg0_p != NULL && cfg1_p != NULL);

    val = sizeof(csk_i2s_samp_rates) / sizeof(csk_i2s_samp_rates[0]);
    for (i = 0; i < val; i++) {
        if (samp_rate == csk_i2s_samp_rates[i])
            break;
    }
    if (i == val) {
        CLOGW("%s: Sample rate is %d, NOT in built-in supported freq list!!\r\n",
                __func__, samp_rate);
    }

#if IC_BOARD // IC_BOARD = 1
    // ONLY 24MHz XTAL is supported on ARCS
    clk_in = XTAL_FREQ();

#else // IC_BOARD = 0
    //FIXME: use postdiv_audclk_sel in audpll_ctrl instead of 2?
    //the clock should be fixed at 24Mhz!
    //clk_in = XTAL_FREQ() / 2;
    clk_in = XTAL_FREQ();
#endif
    LOGD("I2S_MCLK = %dHz", clk_in);

    if (i2s->ch_cfg.tdm_cnt == 0) { // I2S 2 L/R channels (I2S_CLKIN_24MHZ_DEF)
        // calculate padding BCK count
/*
        assert(i2s->ch_cfg.data_len < bck_factors[1]); // bck_factors[1] = 50
        if (i2s->ch_cfg.data_len <= bck_factors[0] - BCK_MARGIN) // bck_factors[0] = 25
            i2s->info->pad_bcks = bck_factors[0] - i2s->ch_cfg.data_len; // 1 channel BCK count: 25
        else
            i2s->info->pad_bcks = bck_factors[1] - i2s->ch_cfg.data_len; // 1 channel BCK count: 50

        val = samp_rate * 2 * (i2s->ch_cfg.data_len + i2s->info->pad_bcks);
        div = clk_in / val;
        if (clk_in % val != 0 || div > BCK_DIV_MAX) {
            CLOGE("%s: Sample rate (%d) CANNOT be generated via frequency division !!\r\n", __func__, samp_rate);
            return false;
        }
*/
        assert(i2s->ch_cfg.data_len < 50);
        for (i = 0; i < BCKF_CNT_FOR_NON_TDM; i++) {
            if (i2s->ch_cfg.data_len > bck_factors[i] - BCK_MARGIN)
                continue;
            val = samp_rate * 2 * bck_factors[i];
            div = clk_in / val;
            if (clk_in % val != 0 || div > BCK_DIV_MAX)
                continue;
            i2s->info->pad_bcks = bck_factors[i] - i2s->ch_cfg.data_len;
            break;
        }

        if (i >= BCKF_CNT_FOR_NON_TDM || i2s->info->pad_bcks > BCK_LRCK_MASK) {
            CLOGE("%s: Sample rate (%d) CANNOT be generated via frequency division !!\r\n", __func__, samp_rate);
            return false;
        }

        cfg0_p->bit.BCK_LRCK = i2s->info->pad_bcks & BCK_LRCK_MASK;
        cfg1_p->bit.SLOT_LRCK = 0;

    } else { // PCM mode 0/1 (TDM mode) (I2S_CLKIN_24MHZ_DEF)
        // calculate padding BCK count
        // make sure the freq_div is an integer:
        // freq_div = clk_in / ( all_chs_bcks * sample_rate )
        // above mentioned: all_chs_bcks = tdm_cnt * data_len + pad_bcks
        if (clk_in % samp_rate != 0) {
            CLOGE("%s: Sample rate (%d) CANNOT be generated via frequency division !!\r\n",
                    __func__, samp_rate);
            return false;
        }

        val = clk_in / samp_rate;
        val2 = i2s->ch_cfg.tdm_cnt * i2s->ch_cfg.data_len + BCK_MARGIN;
        for (i = 0; i < BCKF_CNT; i++) {
            if (val2 > bck_factors[i] || val % bck_factors[i] != 0)
                continue;
            div = val / bck_factors[i];
            if (div <= BCK_DIV_MAX && div > 0) {
                i2s->info->pad_bcks = bck_factors[i] - val2 + BCK_MARGIN;
                break;
            }
        }

        if (i >= BCKF_CNT || i2s->info->pad_bcks > SLOT_LRCK_MASK) {
            CLOGE("%s: Sample rate (%d) CANNOT be generated via frequency division !!\r\n",
                    __func__, samp_rate);
            return false;
        }

        cfg0_p->bit.BCK_LRCK = 0;
        cfg1_p->bit.SLOT_LRCK = i2s->info->pad_bcks & SLOT_LRCK_MASK;
    }

    //NOTE: NO +1 operation for BCK_DIV field on ARCS!!
    //cfg1_p->bit.BCK_DIV = (div - 1) & BCK_DIV_MASK;
    cfg1_p->bit.BCK_DIV = div & BCK_DIV_MASK;
    cfg1_p->bit.BCK_DIV_LD = 1;

    i2s->info->samp_freq = samp_rate;
    return true;
}

static int32_t i2s_get_samp_rate(I2S_DEV *i2s)
{
    assert(i2s != NULL);

    if (!(i2s->info->flags & I2S_FLAG_CONFIGURED)) {
        LOGD("%s: I2S%d has NOT been configured!!",
                __func__, i2s->dev_idx);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->samp_freq != 0)
        return i2s->info->samp_freq;

    uint32_t clk_in, val;

    // ONLY 24MHz XTAL is supported on ARCS
    //clk_in = get_audpll_audio_clk();
    clk_in = XTAL_FREQ();

    val = i2s->reg->REG_I2S_CFG1.bit.BCK_DIV + 1;

    if (i2s->ch_cfg.tdm_cnt == 0) { // I2S 2 L/R channels
        val *= 2 * (i2s->ch_cfg.data_len + i2s->info->pad_bcks);
    } else { // PCM mode 0/1 (TDM mode)
        val *= i2s->ch_cfg.tdm_cnt * i2s->ch_cfg.data_len + i2s->info->pad_bcks;
    }

    return (clk_in / val);
}

static int32_t i2s_set_echo_params(I2S_DEV *i2s, ECHO_PARAMS *params)
{
    int32_t ret;

    assert(i2s != NULL);
    if (params == NULL) {
        LOGD("%s: ECHO parameters SHOULD NOT be NULL!\n", __func__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (i2s->apc_res.support_echo == 0 || i2s->ch_cfg.chbmp_echo == 0) {
        LOGD("%s: SHOULD support ECHO (support_echo=%d) and acquire ECHO channels (chbmp_echo=0x%x) before this\n",
                __func__, i2s->apc_res.support_echo, i2s->ch_cfg.chbmp_echo);
        return CSK_DRIVER_ERROR;
    }

    if (i2s->info->samp_freq == 0 || i2s->ch_cfg.data_len == 0) {
        LOGD("%s: SHOULD set samp_freq (%d) and data_len (%d) before this\n",
                __func__, i2s->info->samp_freq, i2s->ch_cfg.data_len);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH apc_dch = i2s->apc_res.dch_echo;
    ret = apc_dual_channel_setup(apc_dch, i2s->info->apc_chmode,
                                i2s->ch_cfg.chbmp_echo,
                                params->echo_mixed,
                                params->trim_16bits);
    if (ret != CSK_DRIVER_OK)
        return ret;

    ret = apc_echo_setup(apc_dch, i2s->info->samp_freq,
                        params->samp_rate,
                        1, i2s->ch_cfg.chbmp_echo); // support auto_feed
    if (ret != CSK_DRIVER_OK)
        return ret;

    i2s->ch_cfg.mix_echo = params->echo_mixed ? 1 : 0;

    return CSK_DRIVER_OK;
}

static void i2s_reset(I2S_DEV *i2s)
{
    assert(i2s != NULL);
    i2s->reg->REG_I2S_CFG0.bit.SW_RESET = 1; // [RW] set 1 to reset
    //apc_reset_path(0x3); //reset both RX & TX

    volatile uint32_t delay = 100;
    while (delay-- > 0);
    i2s->reg->REG_I2S_CFG0.bit.SW_RESET = 0; // [RW] restore to 0!
}


/**
 \fn          int32_t I2S_Control (void *i2s_dev, uint32_t control, uint32_t arg)
 \brief       Control I2S Interface.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[in]   control  Operation
 \param[in]   arg  Argument of operation (optional), i.e. the speed of i2s when as master
 \return      common \ref execution_status and driver specific \ref i2s execution_status
*/
int32_t
I2S_Control(void *i2s_dev, uint32_t control, uint32_t arg)
{
    I2S_CFG0 cfg0;
    I2S_CFG1 cfg1;
    uint32_t val, tdm_chs;
    bool is_slave;
    uint8_t byval;

    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL) {
        LOGD("%s: invalid I2S device (0x%08x)!", __func__, i2s_dev);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (!(i2s->info->flags & I2S_FLAG_POWERED))
        return CSK_DRIVER_ERROR;

    cfg0.all = i2s->reg->REG_I2S_CFG0.all;
    cfg1.all = i2s->reg->REG_I2S_CFG1.all;

    // exclusive MISC OP
    switch (control & CSK_I2S_EXCL_OP_Msk) {
        // NO MISC OP
        case CSK_I2S_EXCL_OP_UNSET:
            break;

        // abort I2S transfer (including IN & OUT channels)
        case CSK_I2S_ABORT_TRANSFER:
            I2S_Abort_Channels(i2s, (arg & 0xFF), ((arg >> 8) & 0xFF), ((arg >> 16) & 0xFF));
            return CSK_DRIVER_OK;

        // set sample rate, arg = sample_rate_value
        case CSK_I2S_SET_SAMP_RATE:
            if (arg == 0U)
                return CSK_DRIVER_ERROR;
            if (i2s_set_samp_rate(i2s, arg, &cfg0, &cfg1)) {
                i2s->reg->REG_I2S_CFG0.all = cfg0.all;
                i2s->reg->REG_I2S_CFG1.all = cfg1.all;
                return CSK_DRIVER_OK;
            }
            return CSK_DRIVER_ERROR;

        // get sample rate
        case CSK_I2S_GET_SAMP_RATE:
            return i2s_get_samp_rate(i2s);

        // Set ECHO parameters
        case CSK_I2S_SET_ECHO_PARAMS:
            return i2s_set_echo_params(i2s, (ECHO_PARAMS*)arg);

        // Reset I2S module
        case CSK_I2S_RESET:
            i2s_reset(i2s);
            return CSK_DRIVER_OK;

        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
    } // end exclusive MISC_OP

    //TODO: check busy status correctly...
    //if (i2s->info->status.bit.tx_busy || i2s->info->status.bit.rx_busy || i2s->info->status.bit.ech_busy)
    if (i2s->info->status.all & CSK_I2S_STATUS_BUSY_MASK)
        return CSK_DRIVER_ERROR_BUSY;

    tdm_chs = (control & CSK_I2S_TDM_CHS_Msk) >> CSK_I2S_TDM_CHS_Pos;
    i2s->ch_cfg.tdm_cnt = tdm_chs & 0xFF;

    val = (control & CSK_I2S_MODE_Msk);
    if (val == CSK_I2S_MODE_SLAVE)
        is_slave = true;
    else if (val == CSK_I2S_MODE_MASTER)
        is_slave = false;
    else
        is_slave = (cfg0.bit.MASTER_MODE == 0);

    // I2S Protocol and TDM mode
    val = (control & CSK_I2S_PROTO_Msk) >> CSK_I2S_PROTO_Pos;
    if (val > 0 && val <= CSK_I2S_PROTO_LAST_INDEX) {
        cfg0.bit.TX_DLY = 0; // USED in master mode
        cfg0.bit.RX_DLY = 0; // USED in master mode
        cfg0.bit.TXRX_DLY_S = 0; // ONLY for slave mode
        cfg0.bit.TX_HALF_CYCLE_DLY = (is_slave ? 1 : 0);
        cfg0.bit.BCKOUT_GATE = 0; //1; // 1 = NO BCK clock after data has been sent
        cfg0.bit.BCK_FORCE_ON = 1; //FIXME: make I2S master's BCK occurs before LRCK
        //BYPASS_FIFOVLD = 1, indicates that i2s can be enabled regardless of TX FIFO valid
        //FIXME: according to experiments I2S slave TX/RX works only if BYPASS_FIFOVLD = 1!
        // And BYPASS_FIFOVLD SHOULD be set to 1 when only I2S RX is used...
        cfg0.bit.BYPASS_FIFOVLD = ((is_slave || (i2s->ch_cfg.chbmp_out == 0)) ? 1 : 0);

        cfg0.bit.RX_HALF_CYCLE_DLY = 0; // FIXME: when to set 1?
        cfg0.bit.BCK_POL = BCK_POL_INVERT; // BCK_POL_NORMAL
        cfg0.bit.LRCK_POL = LRCK_POL_LEFT_H; // generally set 0 except I2S PHILIPS
        cfg0.bit.RIGHT_JUSTIFIED = 0; // generally set 0 except right_justified
        //cfg0.bit.LSB = 0; // Set by user from control parameter
        //cfg1.bit.BCK_SAME_EDGE = 0; // edge of internal bck and bckout are inverse by default
        cfg1.bit.LONGSYNC = 0; // FIXME: when to set 1?
        cfg1.bit.SLOTNUM = tdm_chs & 0xFF;

        switch (control & CSK_I2S_PROTO_Msk) {
        // Keep mode unchanged
        case CSK_I2S_PROTO_UNSET:
            //CANNOT come here!!
            break;

        case CSK_I2S_PROTO_PHILIPS: // I2S Justified Mode(Philips)
            cfg0.bit.SERIAL_MODE = SERMODE_I2S;
            if (tdm_chs > 0) {
                CLOGE("%s: TDM (tdm_chs=%d) CANNOT be supported in I2S Justified Mode(Philips)!",
                        __func__, tdm_chs);
                return CSK_DRIVER_ERROR_PARAMETER;
            }

            if (is_slave) {
                cfg0.bit.TXRX_DLY_S = 1;
            } else {
                //TODO: the design is confusing...
                // TX_DLY works for both TX & RX? RX_DLY works for RX only?
                cfg0.bit.TX_DLY = 1;
            }

            cfg0.bit.LRCK_POL = LRCK_POL_LEFT_L;
            i2s->info->protocol = I2S_PROTO_PHILIPS;
            break;

        case CSK_I2S_PROTO_LEFT: // Left Justified Mode
            cfg0.bit.SERIAL_MODE = SERMODE_I2S;
            if (tdm_chs > 0) {
                CLOGE("%s: TDM (tdm_chs=%d) CANNOT be supported in Left Justified Mode!",
                        __func__, tdm_chs);
                return CSK_DRIVER_ERROR_PARAMETER;
            }

            i2s->info->protocol = I2S_PROTO_LEFT;
            break;

        case CSK_I2S_PROTO_RIGHT: // Right Justified Mode
            cfg0.bit.SERIAL_MODE = SERMODE_I2S;
            if (tdm_chs > 0) {
                CLOGE("%s: TDM (tdm_chs=%d) CANNOT be supported in Right Justified Mode!",
                        __func__, tdm_chs);
                return CSK_DRIVER_ERROR_PARAMETER;
            }

            //cfg0.bit.BCK_FORCE_ON = 1; //FIXME:
            cfg0.bit.RIGHT_JUSTIFIED = 1;
            i2s->info->protocol = I2S_PROTO_RIGHT;
            break;

        case CSK_I2S_PROTO_PCMMODE_0: // DSP/PCM mode 0
            cfg0.bit.SERIAL_MODE = SERMODE_VOICE;
            if (tdm_chs == 0) {
                CLOGE("%s: no TDM channels in DSP/PCM mode 0!", __func__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }

            if (is_slave) {
                cfg0.bit.TXRX_DLY_S = 1;

                //cfg0.bit.EN_FORCE_ON = 1; //TEST ONLY!!
            } else {
                // According to I2S IP, in the case of PCM mode 0,
                // RX_DLY SHOULD be set 1 if RX only,
                // only TX_DLY SHOULD be set 1 and RX_DLY set 0 if TX&RX both exist
                // (TX_DLY work for both TX and RX! 2 cycles are delayed if both TX_DLY & RX_DLY are set to 1!)
                //
                //cfg0.bit.RX_DLY = 1;
                cfg0.bit.TX_DLY = 1;

                //FIXME: I2S TX_HALF_CYCLE_DLY, I2S BCK_POL, or CODEC BCK_POL should set to 1!
                //      Or else sound noise will occur...
                //cfg0.bit.TX_HALF_CYCLE_DLY = 1;
                //cfg0.bit.BCK_POL = BCK_POL_INVERT;
            }

            i2s->info->protocol = I2S_PROTO_PCMMODE_0;
            break;

        case CSK_I2S_PROTO_PCMMODE_1: // DSP/PCM mode 1
            cfg0.bit.SERIAL_MODE = SERMODE_VOICE;
            if (tdm_chs == 0) {
                CLOGE("%s: no TDM channels in DSP/PCM mode 1!", __func__);
                return CSK_DRIVER_ERROR_PARAMETER;
            }

            i2s->info->protocol = I2S_PROTO_PCMMODE_1;
            break;

        default:
            return CSK_I2S_ERROR_PROTOCOL;
        }
    } // end if !CSK_I2S_PROTO_UNSET

    // Data format
    switch (control & CSK_I2S_DATA_FORMAT_Msk) {
    // Keep data format unchanged
    case CSK_I2S_DATA_FORMAT_UNSET:
        break;

    case CSK_I2S_DATA_FORMAT_DUAL_16BIT:
        cfg0.bit.WLEN = WLEN_16BIT;
        i2s->ch_cfg.data_len = 16;
        i2s->info->apc_chmode = APC_CHMODE_16BITS;
        break;

    case CSK_I2S_DATA_FORMAT_24BIT_HIGH:
        cfg0.bit.WLEN = WLEN_24BIT;
        i2s->ch_cfg.data_len = 24;
        i2s->info->apc_chmode = APC_CHMODE_24BITS_HIGH;
        break;

    case CSK_I2S_DATA_FORMAT_32BIT:
        cfg0.bit.WLEN = WLEN_32BIT;
        i2s->ch_cfg.data_len = 32;
        i2s->info->apc_chmode = APC_CHMODE_32BITS;
        break;

    case CSK_I2S_DATA_FORMAT_24BIT_LOW:
        cfg0.bit.WLEN = WLEN_24BIT;
        i2s->ch_cfg.data_len = 24;
        i2s->info->apc_chmode = APC_CHMODE_24BITS_LOW;
        break;

    case CSK_I2S_DATA_FORMAT_20BIT_HIGH:
        cfg0.bit.WLEN = WLEN_20BIT;
        i2s->ch_cfg.data_len = 20;
        //BSD: According to IC designer, APC_CHMODE_24BITS_HIGH or
        // APC_CHMODE_32BITS should both work for 20BIT_HIGH!
        // We use APC_CHMODE_32BITS here for both 20BIT_HIGH...
        //i2s->info->apc_chmode = APC_CHMODE_24BITS_HIGH;
        i2s->info->apc_chmode = APC_CHMODE_32BITS;
        break;

    //BSD: According to IC designer, 20BIT_LOW is NOT supported!!
    /*
    case CSK_I2S_DATA_FORMAT_20BIT_LOW:
        cfg0.bit.WLEN = WLEN_20BIT;
        i2s->ch_cfg.data_len = 20;
        //i2s->info->apc_chmode = APC_CHMODE_24BITS_LOW;
        i2s->info->apc_chmode = APC_CHMODE_32BITS;
        break;
    */

    default:
        return CSK_I2S_ERROR_DATA_FORMAT;
    }

    // RX Channel Configuration
    val = control & CSK_I2S_RXCH_Msk;
    switch (val) {
    // Keep RX channel configuration unchanged
    case CSK_I2S_RXCH_UNSET:
        break;

    case CSK_I2S_RXCH_SEPA:
        i2s->ch_cfg.mix_in = 0;
        i2s->ch_cfg.trim16_in = 0;
         break;

    case CSK_I2S_RXCH_MIXED:
        i2s->ch_cfg.mix_in = 1;
        i2s->ch_cfg.trim16_in = 0;
        break;

    case CSK_I2S_RXCH_SEPA_TRIM16:
    case CSK_I2S_RXCH_MIXED_TRIM16:
        i2s->ch_cfg.mix_in = (val == CSK_I2S_RXCH_MIXED_TRIM16) ? 1 : 0;
        if (i2s->info->apc_chmode == APC_CHMODE_32BITS ||
            i2s->info->apc_chmode == APC_CHMODE_24BITS_HIGH) {
            i2s->ch_cfg.trim16_in = 1;
        } else {
            LOGD("SHOULD trim 16bit only for data format: 24BITS_HIGH or 32BITS!\r\n");
            i2s->ch_cfg.trim16_in = 0;
            if (i2s->info->apc_chmode == APC_CHMODE_24BITS_LOW)
                return CSK_I2S_ERROR_RXCH_CONFIG;
        }
        break;

    default:
        return CSK_I2S_ERROR_RXCH_CONFIG;
    }

    // TX Channel Configuration
    val = control & CSK_I2S_TXCH_Msk;
    switch (val) {
    // Keep TX channel configuration unchanged
    case CSK_I2S_TXCH_UNSET:
        break;

    case CSK_I2S_TXCH_MONO_SRC_MONO:
        i2s->ch_cfg.mix_out = 0;
        //if tx_mode is set to MONO_IN_MONO_OUT, the mono L/R channel's data
        // is transferred on both LRCK=high and LRCK=low. Now we just request
        // data is transfered in half of LRCK cycle (high or low), and idle in the other half.
        // So the STEREO_STEREO tx_mode is used instead...
        //
        //i2s->ch_cfg.tx_mode = TXMODE_MONO_MONO;
        //cfg0.bit.TX_MODE = TXMODE_MONO_MONO;
        i2s->ch_cfg.tx_mode = TXMODE_STEREO_STEREO;
        cfg0.bit.TX_MODE = TXMODE_STEREO_STEREO;
        i2s->ch_cfg.expd16_out = 0;
        break;

    case CSK_I2S_TXCH_STEREO_SRC_MONO:
        i2s->ch_cfg.mix_out = 0;
        i2s->ch_cfg.tx_mode = TXMODE_MONOL_STEREO; // via LEFT channel by default
        cfg0.bit.TX_MODE = TXMODE_MONOL_STEREO;
        i2s->ch_cfg.expd16_out = 0;
        break;

    case CSK_I2S_TXCH_STEREO_SRC_STEREO:
        i2s->ch_cfg.mix_out = 1;
        i2s->ch_cfg.tx_mode = TXMODE_STEREO_STEREO;
        cfg0.bit.TX_MODE = TXMODE_STEREO_STEREO;
        i2s->ch_cfg.expd16_out = 0;
        break;

    case CSK_I2S_TXCH_MONO_SRC_MONO_EXPD16:
    case CSK_I2S_TXCH_STEREO_SRC_MONO_EXPD16:
    case CSK_I2S_TXCH_STEREO_SRC_STEREO_EXPD16:
        i2s->ch_cfg.mix_out = (val == CSK_I2S_TXCH_STEREO_SRC_STEREO_EXPD16 ? 1 : 0);
        i2s->ch_cfg.tx_mode = cfg0.bit.TX_MODE =
                        (val == CSK_I2S_TXCH_STEREO_SRC_MONO_EXPD16 ?
                        TXMODE_MONOL_STEREO : TXMODE_STEREO_STEREO);
        if (i2s->info->apc_chmode == APC_CHMODE_32BITS ||
            i2s->info->apc_chmode == APC_CHMODE_24BITS_HIGH) {
            i2s->ch_cfg.expd16_out = 1;
        } else {
            LOGD("SHOULD trim 16bit only for data format: 24BITS_HIGH or 32BITS!\r\n");
            i2s->ch_cfg.expd16_out = 0;
            if (i2s->info->apc_chmode == APC_CHMODE_24BITS_LOW)
                return CSK_I2S_ERROR_TXCH_CONFIG;
        }
        break;

    default:
        return CSK_I2S_ERROR_TXCH_CONFIG;
    }

    // Bit Order Configuration
    switch (control & CSK_I2S_BIT_ORDER_Msk) {
    // Keep Bit Order configuration unchanged
    case CSK_I2S_BIT_ORDER_UNSET:
        break;

    case CSK_I2S_BIT_ORDER_MSB:
        cfg0.bit.LSB = 0;
        break;

    case CSK_I2S_BIT_ORDER_LSB:
        cfg0.bit.LSB = 1;
        break;

    default:
        return CSK_I2S_ERROR_BIT_ORDER;
    }

    // SWAP L/R channel Configuration
    switch (control & CSK_I2S_SWAP_CHLR_Msk) {
    // Keep SWAP_CHLR configuration unchanged
    case CSK_I2S_SWAP_CHLR_UNSET:
        break;

    case CSK_I2S_SWAP_CHLR_IN: // SWAP IN
        cfg0.bit.SWAP_CHLR_IN = 1;
        break;

    case CSK_I2S_SWAP_CHLR_OUT: // SWAP IN
        cfg0.bit.SWAP_CHLR_OUT = 1;
        break;

    case CSK_I2S_SWAP_CHLR_BOTH: // SWAP IN & OUT
        cfg0.bit.SWAP_CHLR_IN = 1;
        cfg0.bit.SWAP_CHLR_OUT = 1;
        break;

    case CSK_I2S_SWAP_CHLR_IN_NOT: // DON'T SWAP IN
        cfg0.bit.SWAP_CHLR_IN = 0;
        break;

    case CSK_I2S_SWAP_CHLR_OUT_NOT: // DON'T SWAP OUT
        cfg0.bit.SWAP_CHLR_OUT = 0;
        break;

    case CSK_I2S_SWAP_CHLR_NONE: // DON'T SWAP IN & OUT
        cfg0.bit.SWAP_CHLR_IN = 0;
        cfg0.bit.SWAP_CHLR_OUT = 0;
        break;

    default:
        return CSK_I2S_ERROR_BIT_ORDER;
    }

    // call apc_dual_channel_setup to notify APC channels
    int32_t ret;
    APC_DCH apc_dch;
    if (i2s->ch_cfg.chbmp_in != 0) {
    #if I2S_IN_USE_PIO
        uint8_t ch_flag = i2s->ch_cfg.trim16_in | 0x2; //use PIO for TEST!
        LOGD("%s: used PIO to read RX FIFO!", __func__);
    #else
        uint8_t ch_flag = i2s->ch_cfg.trim16_in;
    #endif
        apc_dch = i2s->apc_res.dch_in;
        ret = apc_dual_channel_setup(apc_dch, i2s->info->apc_chmode,
                APC_DCH_BMP_STEREO, //tdm_chs ? APC_DCH_BMP_LEFT : APC_DCH_BMP_STEREO,
                i2s->ch_cfg.mix_in,
                ch_flag);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    if (i2s->ch_cfg.chbmp_out != 0) {
    #if I2S_OUT_USE_PIO
        uint8_t ch_flag = i2s->ch_cfg.expd16_out | 0x2; //use PIO for TEST!
        LOGD("%s: used PIO to write TX FIFO!", __func__);
    #else
        uint8_t ch_flag = i2s->ch_cfg.expd16_out;
    #endif
        apc_dch = i2s->apc_res.dch_out;
        ret = apc_dual_channel_setup(apc_dch, i2s->info->apc_chmode,
                APC_DCH_BMP_STEREO, //tdm_chs ? APC_DCH_BMP_LEFT : APC_DCH_BMP_STEREO,
                i2s->ch_cfg.mix_out,
                ch_flag);
        if (ret != CSK_DRIVER_OK)
            return ret;
    }

    // I2S Mode
    val = (control & CSK_I2S_MODE_Msk);
    switch (val) {
    // Keep mode unchanged
    case CSK_I2S_MODE_UNSET:
        break;

    // I2S master (output clocks, i.e. BCK, LRCK, MCK etc.), arg = sample_rate
    case CSK_I2S_MODE_MASTER:
        // set master mode
        cfg0.bit.MASTER_MODE = 1;
        i2s->info->master = 1;
        i2s->info->status.bit.master = 1;

        byval = I2S_CLKIN(arg);
        i2s->info->clkin_idx = byval;
        //i2s->info->status.bit.clkin_idx = byval;

        if (arg != 0) {
            // padding BCK count & frequency DIV for specified sample rate
            // are both calculated in i2s_set_samp_rate()!
            if (!i2s_set_samp_rate(i2s, I2S_SAMP_RATE(arg), &cfg0, &cfg1))
                return CSK_DRIVER_ERROR_PARAMETER;
        }
        i2s->info->flags |= I2S_FLAG_CONFIGURED;

        break;

    // I2S slave (input clocks from the counterpart)
    case CSK_I2S_MODE_SLAVE:
        // set slave mode
        cfg0.bit.MASTER_MODE = 0;
        i2s->info->master = 0;
        i2s->info->status.bit.master = 0;

        i2s->info->clkin_idx = I2S_CLKIN_24MHZ_DEF;
        //i2s->info->status.bit.clkin_idx = I2S_CLKIN_24MHZ_DEF;

        if (i2s->info->protocol == I2S_PROTO_RIGHT) {
            // arg is supposed to be BCK / LRCK, should be even
            // use it to evaluate cfg0.bit.BCK_LRCK...
            if (arg != 0) {
                if ((arg & 0x1) || arg < i2s->ch_cfg.data_len * 2)
                    return CSK_DRIVER_ERROR_PARAMETER;
                cfg0.bit.BCK_LRCK = i2s->info->pad_bcks = (arg >> 1) - i2s->ch_cfg.data_len;
            }
        } else { // reset to 0 if other protocols in slave mode
            cfg0.bit.BCK_LRCK = cfg1.bit.SLOT_LRCK = i2s->info->pad_bcks = 0;
        }
        i2s->info->flags |= I2S_FLAG_CONFIGURED;
        break;

    default:
        return CSK_I2S_ERROR_MODE;
    } // end I2S Mode

    // write protocols & tdm settings into cfg0/1 registers
    i2s->reg->REG_I2S_CFG0.all = cfg0.all;
    i2s->reg->REG_I2S_CFG1.all = cfg1.all;
//    i2s->reg->REG_I2S_CFG1.all = cfg1.all; // FIXME: make sure BCK_DIV_LD takes effect!

    return CSK_DRIVER_OK;
}

/**
 \fn          int32_t I2S_GetStatus (void *spi_dev, CSK_I2S_STATUS *status)
 \brief       Get I2S status.
 \param[in]   i2s_dev  Pointer to I2S device instance
 \param[out]  status  Pointer to CSK_I2S_STATUS buffer
 \return      \ref execution_status
 */
int32_t
I2S_GetStatus(void *i2s_dev, CSK_I2S_STATUS *status)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);

    if (i2s == NULL || status == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;

    (*status).all = i2s->info->status.all;
    return CSK_DRIVER_OK;
}


int32_t I2S_EQ_Set_Coef_Array(void *i2s_dev, uint32_t *eqcoefs, uint32_t num)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_set_coef_array(i2s->apc_res.dch_out, eqcoefs, num);
}

int32_t I2S_EQ_Set_Coef(void *i2s_dev, uint32_t index, uint32_t eqcoef)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_set_coef(i2s->apc_res.dch_out, index, eqcoef);
}

int32_t I2S_EQ_Enable(void *i2s_dev, uint32_t stages)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_enable(i2s->apc_res.dch_out, stages);
}

int32_t I2S_EQ_Disble(void *i2s_dev)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_disble(i2s->apc_res.dch_out);
}

int32_t I2S_EQ_Clear(void *i2s_dev, uint8_t wait_done)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    if (i2s == NULL)
        return CSK_DRIVER_ERROR_PARAMETER;
    return apc_eq_clear(i2s->apc_res.dch_out, wait_done);
}


uint32_t i2s_rx_cnt_on_bus(void *i2s_dev)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    assert(i2s != NULL);

    uint8_t bmp = i2s->ch_cfg.chbmp_in;
    APC_DCH dch = i2s->apc_res.dch_in;
    assert(bmp != 0);

    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO && i2s->ch_cfg.mix_in) {
        return apc_dual_channel_get_count(dch);
    } else if ((bmp & CH_BMP_STEREO) == CH_BMP_LEFT)
        return apc_channel_get_count(APC_DCH_TO_CH(dch, 0));
    else if ((bmp & CH_BMP_STEREO) == CH_BMP_RIGHT)
        return apc_channel_get_count(APC_DCH_TO_CH(dch, 1));
    return 0;
}

uint32_t i2s_tx_cnt_on_bus(void *i2s_dev)
{
    I2S_DEV *i2s = safe_i2s_dev(i2s_dev);
    assert(i2s != NULL);

    uint8_t bmp = i2s->ch_cfg.chbmp_out;
    APC_DCH dch = i2s->apc_res.dch_out;
    assert(bmp != 0);

    int tx_cnt = 0;
    uint32_t samp_cnt = apc_get_fifo_samp_cnt(dch, bmp);
    if ((bmp & CH_BMP_STEREO) == CH_BMP_STEREO && i2s->ch_cfg.mix_out) {
        tx_cnt = apc_dual_channel_get_count(dch) - samp_cnt;
    } else if (bmp & CH_BMP_LEFT) {
        tx_cnt = apc_channel_get_count(APC_DCH_TO_CH(dch, 0)) - samp_cnt;
    } else if (bmp & CH_BMP_RIGHT) {
        tx_cnt = apc_channel_get_count(APC_DCH_TO_CH(dch, 1)) - samp_cnt;
    }

    if (tx_cnt < 0)
        tx_cnt = 0;
    return tx_cnt;
}


_FAST_FUNC_RO static void
i2s_apc_event(uint32_t event_info, uint32_t usr_param)
{
    uint8_t ch_dir, notify = 1;
    uint32_t i2s_event_info;

    uint8_t event_type = event_info & 0xFF;
    APC_CH apc_ch = (event_info >> 8) & 0xFF;
    I2S_DEV *i2s = (I2S_DEV *)usr_param;
    assert(i2s != NULL);

    apc_dual_channel_owner(APC_CH_TO_DCH(apc_ch), NULL, NULL, &ch_dir);
    i2s_event_info = (APC_CH_LR_IDX(apc_ch) << CSK_I2S_EVENT_I2S_CH_POS) |
            (APC_CH_TO_DCH(apc_ch) << CSK_I2S_EVENT_APC_DCH_POS);
    if (ch_dir != 0)
        i2s_event_info |= 1 << CSK_I2S_EVENT_XFER_DIR_POS;

    if (event_type & APC_EVENT_TRANSFER_COMPLETE) {
        if (ch_dir) { // IN
            if (i2s->ch_cfg.chbmp_echo != 0 && APC_CH_TO_DCH(apc_ch) == i2s->apc_res.dch_echo) {
                i2s_event_info |= CSK_I2S_EVENT_ECHO_RX_COMPLETE;
                i2s->info->status.bit.ech_busy = 0;
            } else {
                i2s_event_info |= CSK_I2S_EVENT_RECEIVE_COMPLETE;
                i2s->info->status.bit.rx_busy = 0;
            }
        } else { // OUT
            i2s->info->status.bit.tx_busy = 0;
            i2s_event_info |= CSK_I2S_EVENT_TRANSMIT_COMPLETE;
        }
    }

    if (event_type & APC_EVENT_PIPO_DONE) { // Ping/Pong Transfer Done
        if (event_info & PIPO_PING_XFER_DONE)
            i2s_event_info |= ch_dir ? CSK_I2S_EVENT_RX_PING_DONE : CSK_I2S_EVENT_TX_PING_DONE;
        if (event_info & PIPO_PONG_XFER_DONE)
            i2s_event_info |= ch_dir ? CSK_I2S_EVENT_RX_PONG_DONE : CSK_I2S_EVENT_TX_PONG_DONE;
    }

    if (event_type & APC_EVENT_I2S_ERROR) {
        //TODO: Upper layer SHOULD reset I2S module by calling Control with CSK_I2S_RESET!!
        i2s_event_info |= CSK_I2S_EVENT_CLOCK_ERROR;
    }

    if (event_type & APC_EVENT_RX_FIFO_OVERRUN) {
        if (i2s->ch_cfg.chbmp_echo != 0 && APC_CH_TO_DCH(apc_ch) == i2s->apc_res.dch_echo) {
            i2s->info->status.bit.ech_ovf = 1;
            i2s_event_info |= CSK_I2S_EVENT_ECHO_RX_FIFO_OVERRUN;
        } else {
            i2s->info->status.bit.rx_ovf = 1;
            i2s_event_info |= CSK_I2S_EVENT_RX_FIFO_OVERRUN;
        }
    }

    if (event_type & APC_EVENT_TX_FIFO_UNDERRUN) {
        i2s->info->status.bit.tx_unf = 1;
        i2s_event_info |= CSK_I2S_EVENT_TX_FIFO_UNDERRUN;
    }

    if (event_type & APC_EVENT_RX_FIFO_FULL) {
        i2s_event_info |= CSK_I2S_EVENT_RX_FIFO_FULL;
    }

    if (event_type & APC_EVENT_TX_FIFO_EMPTY) {
        i2s_event_info |= CSK_I2S_EVENT_TX_FIFO_EMPTY;
    }

    if (event_type & APC_EVENT_DMA_ERROR) {
        i2s_event_info |= CSK_I2S_EVENT_OTHER_ERROR;
    }

    // notify I2S caller
    if (notify && i2s->info->cb_event != NULL) {
        i2s->info->cb_event(i2s_event_info, i2s->info->usr_param);
        //BSD: just disable I2S module if no RX, TX, ECHO RX operation
        //!status.bit.tx_busy && !status.bit.rx_busy && !status.bit.ech_busy
        //NOTE: tx_busy = 0 indicates that TX DMA has been completed,
        // it CANNOT make sure data has been sent out on the bus,
        // so it's NOT proper to disable I2S here...
//        if ((i2s->info->status.all & CSK_I2S_STATUS_BUSY_MASK) == 0)
//            disable_i2s(i2s);
    }
}
