/**
 ****************************************************************************************
 * @file aud_dac.c
 *
 * @brief  audio dac source
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "aud_dac.h"
#include "Driver_DAC.h"
#include "log_print.h"

/*
 * MACROS
 ****************************************************************************************
 */
#define EXT_CODEC       0
#define CHANNEL_COUNT   1
#define DAC_BMP         DAC_BMP_LEFT //DAC_BMP_STEREO //
//----------------------------------------------------------------------------
#define DMA_CH_AUD_TX_DEF           ((uint8_t) 2)   // CLASSD/I2S TX
#define DMA_CH_AUD_RX_DEF           ((uint8_t) 1)   // ADC/DMIC/I2S RX
#define DMA_CH_AUD_ECHO_DEF         ((uint8_t) 3)   // CLASSD/I2S TX LOOPBACK

#define CHANNEL_BITS        16 //16 //
#define CHANNEL_BYTES       ((CHANNEL_BITS + 7)/8)

/*
 * DEFINES
 ****************************************************************************************
 */
#define OUT_BLK_CNT         2
#define OUT_RENEW_BLK_CNT   1

#define DEBUG_DAC_SIN       0

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
 
/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
extern void btos_free(void *ptr);
/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
aud_dac_out_env_t dac_env;
PIPO_OUT_BLOCK dac_buf_env[OUT_BLK_CNT] = { 0 };

#ifdef DEBUG_DAC_SIN
uint32_t sin_wav_cnt = 0;
uint16_t sin_wav[10] = {
    0x0000, 0x4B3C, 0x79BC, 0x79BB, 0x4B3B, 0x0000, 0xB4C5, 0x8644, 0x8644,0xB4C4,	
};
#endif

/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */


 bool aud_init_dac_out_pipo(uint16_t* buf_pi, uint16_t* buf_po, uint32_t pi_len, uint32_t po_len)
{
    int32_t ret;
    uint8_t blk_cnt = 2;

    
    CLOGI("init pi:0x%x, 0x%x;po:0x%x, %d", buf_pi, buf_po, pi_len, po_len);
    memset(dac_buf_env, 0, sizeof(dac_buf_env));
    dac_buf_env[0].sample_data = (uint32_t *)buf_pi;
    dac_buf_env[0].sample_cnt = pi_len;

    dac_buf_env[1].sample_data = (uint32_t *)buf_po;
    dac_buf_env[1].sample_cnt = po_len;

    ret = DAC_Send_PiPo(dac_env.dac_handle, dac_buf_env, &blk_cnt, DAC_BMP, DAC_TX_FLAG_START_NOW);
    return (ret == CSK_DRIVER_OK);
}
void * aud_init_dac_out(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_DAC_SignalEvent_t cb_event)
{
    void *dac_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    //if(use_16bits)
    //{
    //    CLOGE("%s: ARCS_C0 not support 16 bit!\r\n", __func__);
    //    use_16bits = 0;
    //}
    // Check sample rate
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_DAC_SR_8KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
//    case 12000:
//        SR_reg_val = CSK_DAC_SR_12KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
    case 16000:
        SR_reg_val = CSK_DAC_SR_16KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
//    case 24000:
//        SR_reg_val = CSK_DAC_SR_24KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
    case 32000:
        SR_reg_val = CSK_DAC_SR_32KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
    case 48000:
        SR_reg_val = CSK_DAC_SR_48KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
    case 96000:
        SR_reg_val = CSK_DAC_SR_96KHZ;
        OSR_reg_val = CSK_DAC_OSR_125;
        break;
//    case 11025:
//        SR_reg_val = CSK_DAC_SR_11P025KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
//    case 22050:
//        SR_reg_val = CSK_DAC_SR_22P05KHZ;
//        OSR_reg_val = CSK_DAC_OSR_250;
//        break;
//    case 44100:
//        SR_reg_val = CSK_DAC_SR_44P1KHZ;
//        OSR_reg_val = CSK_DAC_OSR_125;
//        break;
    default:
        CLOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get DAC device group
    dac_grp = DAC01();
    if (dac_grp == NULL)
        return NULL;

    // FIXME/TODO: DAC P/N use dedicated pins, so NO IOMUX settings are required...

//    uint8_t use_flags = 0;
//    if (use_16bits) use_flags |= DAC_USE_16BITS;
//    iret = DAC_Initialize(dac_grp, cb_event, (uint32_t)dac_grp, dev_bmp, 0, use_flags);

    DAC_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;

    uint8_t dev_bmp_flag = dev_bmp << DAC_BMP_FLAG_OUT_POS; // ADC_PDM_USE_PDM
    if (use_16bits) dev_bmp_flag |= DAC_BMP_FLAG_USE_16BITS;
    iret = DAC_Initialize(dac_grp, cb_event, (uint32_t)dac_grp, dev_bmp_flag, &dma_chs);

    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = DAC_PowerControl(dac_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

//    uint32_t tx_config;
//    if ((dev_bmp & DAC_BMP_STEREO) == DAC_BMP_STEREO)
//        tx_config = CSK_DAC_TXCFG_STEREO_SRC_STEREO;
//    else
//        tx_config = CSK_DAC_TXCFG_MONO_SRC_MONO;

    //TODO: set arg = CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(1)
    iret = DAC_Control(dac_grp, SR_reg_val | OSR_reg_val | CSK_DAC_SOFT_MUTE_SET, // | tx_config
                    CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(0x3)); // arg bit[4:0] = soft mute setting
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Volume settings for L/R DAC devices
    uint32_t gain_a=0, gain_d=0, vol_flag=0;
    if (dev_bmp & DAC_BMP_LEFT) {
//        gain_a |= DAC_GAIN_A_VAL(-6); // -6 dB
//        gain_d |= DAC_GAIN_D_VAL(-20); // -20 dB (-6dB works, but 0dB doesn't work?)
        gain_a |= DAC_GAIN_A_VAL(-18); // -6 dB
        gain_d |= DAC_GAIN_D_VAL(-24); // 6 dB
        vol_flag |= DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT;
    }

/*
    if (dev_bmp & DAC_BMP_RIGHT) {
        gain_a |= DAC_GAIN_A_VAL(-6) << 16; // -6 dB
        gain_d |= DAC_GAIN_D_VAL(-20) << 16; // -20 dB (-6dB works, but 0dB doesn't work?)
//        gain_a |= DAC_GAIN_A_VAL(-6) << 16; // -6 dB
//        gain_d |= DAC_GAIN_D_VAL(6) << 16; // 6 dB
        vol_flag |= DAC_VOL_FLAG_A_RIGHT | DAC_VOL_FLAG_D_RIGHT;
    }
*/

    iret = DAC_SetVolume(dac_grp, gain_a, gain_d, vol_flag);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Mute settings for L/R DAC device
    uint8_t mute_val = 0;
    if (dev_bmp & DAC_BMP_LEFT) {
        //mute_val = DAC_BMP_LEFT;
        mute_val &= ~DAC_BMP_LEFT;
    }
/*
    if (dev_bmp & DAC_BMP_RIGHT) {
        //mute_val = DAC_BMP_RIGHT;
        mute_val &= ~DAC_BMP_RIGHT;
    }
*/

    iret = DAC_SetMute(dac_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

/*  //FIXME: check if PA chip is used...
    // For the Venus6001 QFN64 MPW reference design board,
    // it's necessary to pull-up PA4 to enable DAC.
    // It seems to make no difference now...
    assert(gpio_A != NULL);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 0); //PA4 as GPIO (func=0)
    GPIO_SetDir(gpio_A, CSK_GPIO_PIN4, CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(gpio_A, CSK_GPIO_PIN4, 1);
*/

    CLOGD("%s: DAC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        DAC_Uninitialize(dac_grp);
        return NULL;
    }

    return dac_grp;
}

uint32_t time_cb1 = 0;
uint32_t time_play1 = 0;

void dac_oneshot_play_cb(uint32_t event_info, uint32_t usr_param)
{
    void *dac_grp = (void*)usr_param;
    uint8_t event, ch_no;
    uint8_t status = 0;
    //uint32_t cur_time = bt_time_us_get();
    //CLOGD("cb dur:%d, play to cb:%d", cur_time - time_cb1, cur_time - time_play1);
    //time_cb1 = cur_time;
    
    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;
    if(event & CSK_DAC_EVENT_SEND_COMPLETE){
        //g_playdone = 1;
        int32_t rx_cnt = DAC_GetTxCount(dac_grp, DAC_BMP);
        //LOGD ("%s: DAC (ch_no = %d, rx = %d) Play DONE!!\n",
        //        __func__, ch_no, rx_cnt);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        status = 1;
        //DAC_Abort(dac_grp, DAC_BMP, 0);
        CLOGW("%s: DAC TX FIFO Underflow!!\r\n", __func__);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        status = 2;
        CLOGW("%s: DAC TX FIFO Empty!!\r\n", __func__);
    }

    //CLOGW("play_cb:0x%x,evt:0x%x", dac_env.dac_cfg.play_cb, event);
    if(dac_env.dac_cfg.play_cb && (status == 0))
    {
        dac_env.dac_cfg.play_cb(status);
    }
}

void dac_pi_po_play_cb(uint32_t event_info, uint32_t usr_param)
{
    void *dac_grp = (void*)usr_param;
    uint8_t event, ch_no;
    uint8_t status = 0;
    //uint32_t cur_time = co_time_us_get();
    //CLOGD("cb dur:%d, play to cb:%d", cur_time - time_cb1, cur_time - time_play1);
    //time_cb1 = cur_time;
    
    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;
    if(event & CSK_DAC_EVENT_BLOCK_COMPLETE)
    {
        uint16_t txd_len = 0;
        PIPO_OUT_BLOCK dac_out_blks[OUT_BLK_CNT];
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        memset(dac_out_blks, 0, sizeof(dac_out_blks));
        int32_t ret = DAC_PiPo_Xferred_Blocks(dac_grp, dac_out_blks, 2, DAC_BMP);

        if((dac_env.pi_po_handle == 0) || (dac_env.pi_po_handle == 2))
        {
            dac_env.pi_po_handle = 1;
        }
        else
        {
            dac_env.pi_po_handle = 2;
        }
        txd_len = dac_out_blks[0].sample_cnt * CHANNEL_BYTES;
        //LOGD("DAC BLK DONE, tx %d bytes", txd_len);
    }
    
    if(event & CSK_DAC_EVENT_SEND_COMPLETE){
        //g_playdone = 1;
        int32_t rx_cnt = DAC_GetTxCount(dac_grp, DAC_BMP);
        //LOGD ("%s: DAC (ch_no = %d, rx = %d) Play DONE!!\n",
        //        __func__, ch_no, rx_cnt);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        status = 1;
        //DAC_Abort(dac_grp, DAC_BMP, 0);
        CLOGW("%s: DAC TX FIFO Underflow!!\r\n", __func__);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        status = 2;
        CLOGW("%s: DAC TX FIFO Empty!!\r\n", __func__);
    }

    //CLOGW("play_cb:0x%x,evt:0x%x", dac_env.dac_cfg.play_cb, event);
    if(dac_env.dac_cfg.play_cb && (status == 0))
    {
        dac_env.dac_cfg.play_cb(status);
    }
}

uint16_t dac_dual_to_mono(uint16_t sample_num, uint8_t *pcm)
{
    uint16_t i = 0;
    uint16_t *data = (uint16_t *)pcm;
    for(i = 0; i < sample_num/2; i++)
    {
        data[i] = data[i*2];
#if DEBUG_DAC_SIN
        data[i] = sin_wav[sin_wav_cnt++];
        if(sin_wav_cnt >= 10)
        {
            sin_wav_cnt = 0;
        }
#endif
    }
    return sample_num/2;
}

uint16_t dac_16b_to_32b_dual_to_mono(uint16_t pcm_len, uint8_t *pcm)
{
    uint16_t i = 0;
    uint16_t *data = (uint16_t *)pcm;
    ///left ch set 0 that equal to right << 16.
    for(i = 0; i < pcm_len/4; i++)
    {
        data[2*i] = 0;
        
#if DEBUG_DAC_SIN
        data[2*i + 1] = sin_wav[sin_wav_cnt++];
        if(sin_wav_cnt >= 10)
        {
            sin_wav_cnt = 0;
        }
#endif
    }
    return pcm_len;
}
uint8_t dac_pi_po_play(aud_dac_out_cfg_t *dac_cfg)
{
    int32_t ret = CSK_DRIVER_OK;
    const uint32_t *data;
    uint32_t sample_byte;

    // audio clip comes from static array of prepared audio data, 16Khz,16bit,stereo...

    data = (const uint32_t *)(dac_cfg->play_buf);
    sample_byte = dac_cfg->play_len;
    if(dac_cfg->ch == 2 && dac_cfg->out_bits == 16)
    {
        sample_byte = dac_dual_to_mono(dac_cfg->play_len/2, dac_cfg->play_buf) * 2;
    }
    else
    {
    	ret = CSK_DRIVER_ERROR;
    }

    //CLOGI("play_buf:0x%x,sample_byte:%d,play_len:%d,pipo:%d", data,sample_byte, dac_cfg->play_len, dac_env.pi_po_handle);
    if(dac_env.pi_po_handle == 1)
    {
        memcpy(dac_env.buf_pi, dac_cfg->play_buf, sample_byte);
    }
    else
    {
        memcpy(dac_env.buf_po, dac_cfg->play_buf, sample_byte);
    }

//    data = (const uint32_t *)g_audio_clip1;
//    num = sizeof(g_audio_clip1) / CHANNEL_BYTES;

//    data = (const uint32_t *)g_clip_earthquake_16k16b2ch;
//    num = sizeof(g_clip_earthquake_16k16b2ch) / CHANNEL_BYTES;

//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_STEREO_SRC_STEREO, 0);
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_STEREO, DAC_TX_FLAG_START_NOW);

//    data = (const uint32_t *)g_clip_earthquake_16k16b1ch;
//    num = sizeof(g_clip_earthquake_16k16b1ch) / CHANNEL_BYTES;
//
//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_MONO_SRC_MONO, 0);
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
//    //ret = DAC_Send(g_dac_out, data, num, DAC_BMP_RIGHT, DAC_TX_FLAG_START_NOW);

//    // TXCFG_STEREO_SRC_MONO NOT SUPPORTED YET!!
//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_STEREO_SRC_MONO, 0);
//    // only DAC_BMP_STEREO is supported, and neither DAC_BMP_LEFT nor DAC_BMP_RIGHT is supported!
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_STEREO, DAC_TX_FLAG_START_NOW);

    return (ret == CSK_DRIVER_OK);
}

uint8_t dac_oneshot_play(aud_dac_out_cfg_t *dac_cfg)
{
    int32_t ret;
    const uint32_t *data;
    uint32_t num;

    // audio clip comes from static array of prepared audio data, 16Khz,16bit,stereo...

    data = (const uint32_t *)(dac_cfg->play_buf);
    if(dac_cfg->ch == 2 && dac_cfg->out_bits == 16)
    {
        num = dac_16b_to_32b_dual_to_mono(dac_cfg->play_len, dac_cfg->play_buf) / CHANNEL_BYTES;
    }
    else
    {
        CLOGI("arcs_c0 dac not support this format!ch:%d,out_bits:%d",dac_cfg->ch, dac_cfg->out_bits);
        return 0xff;
    }

    //CLOGW("play_buf:0x%x,num:%d", data,num);

    ret = DAC_Send(dac_env.dac_handle, data, num, DAC_BMP, DAC_TX_FLAG_START_NOW);

//    data = (const uint32_t *)g_audio_clip1;
//    num = sizeof(g_audio_clip1) / CHANNEL_BYTES;

//    data = (const uint32_t *)g_clip_earthquake_16k16b2ch;
//    num = sizeof(g_clip_earthquake_16k16b2ch) / CHANNEL_BYTES;

//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_STEREO_SRC_STEREO, 0);
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_STEREO, DAC_TX_FLAG_START_NOW);

//    data = (const uint32_t *)g_clip_earthquake_16k16b1ch;
//    num = sizeof(g_clip_earthquake_16k16b1ch) / CHANNEL_BYTES;
//
//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_MONO_SRC_MONO, 0);
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
//    //ret = DAC_Send(g_dac_out, data, num, DAC_BMP_RIGHT, DAC_TX_FLAG_START_NOW);

//    // TXCFG_STEREO_SRC_MONO NOT SUPPORTED YET!!
//    DAC_Control(g_dac_out, CSK_DAC_TXCFG_STEREO_SRC_MONO, 0);
//    // only DAC_BMP_STEREO is supported, and neither DAC_BMP_LEFT nor DAC_BMP_RIGHT is supported!
//    ret = DAC_Send(g_dac_out, data, num, DAC_BMP_STEREO, DAC_TX_FLAG_START_NOW);

    return (ret == CSK_DRIVER_OK);
}

uint8_t app_dac_out_init(aud_dac_out_cfg_t *dac_cfg)
{
    CLOGI("dac init, out_bit:%d, ch:%d,sample:%d,pcm len:%d", dac_cfg->out_bits, dac_cfg->ch, dac_cfg->sample_rate, dac_cfg->play_len);
    uint8_t status = 0xff;
    if(dac_cfg->sample_rate ==44100)
    {
        CLOGI("dac not support 44100, resample to 48000");
        dac_cfg->sample_rate= 48000;
    }
    memcpy(&dac_env.dac_cfg, dac_cfg, sizeof(aud_dac_out_cfg_t));
    // Initialize DAC OUT device group, support 24 bits sample (act as 32 bits) only
    dac_env.dac_handle = aud_init_dac_out(DAC_BMP, (dac_cfg->out_bits == 16 ? 1 : 0),
                        dac_cfg->sample_rate, dac_pi_po_play_cb);
    dac_env.pi_po_handle = 0;
    if (dac_env.dac_handle == NULL)
        return -1;
    if(dac_cfg->ch == 2)
    {
        dac_env.sample_len = dac_cfg->play_len/4;
    }
    else
    {
        dac_env.sample_len = dac_cfg->play_len/2;
    }
    dac_env.buf_pi = (uint8_t *)btos_calloc(1, dac_env.sample_len * 2);
    dac_env.buf_po = (uint8_t *)btos_calloc(1, dac_env.sample_len * 2);
    if(dac_env.buf_pi && dac_env.buf_po)
    {
    	status = aud_init_dac_out_pipo((uint16_t *)dac_env.buf_pi, (uint16_t *)dac_env.buf_po, dac_env.sample_len, dac_env.sample_len);
    }
    else
    {
        CLOGI("dac pi po buf not res!");
    }
    return status;
}

uint8_t app_dac_out_play(aud_dac_out_cfg_t *dac_cfg)
{
    //uint32_t cur_time = co_time_us_get();
    //CLOGD("play dur:%d, cb to play:%d", cur_time - time_play1, cur_time - time_cb1);
    //time_play1 = cur_time;
    return dac_pi_po_play(dac_cfg);
}

uint8_t app_dac_out_stop(void)
{
    CLOGI("app_dac_out_stop!");
    uint8_t status = DAC_Uninitialize(dac_env.dac_handle);
    if(dac_env.buf_pi)
    {
        btos_free(dac_env.buf_pi);
        dac_env.buf_pi = NULL;
    }

    if(dac_env.buf_po)
    {
        btos_free(dac_env.buf_po);
        dac_env.buf_po = NULL;
    }
    return status;
}

/// @} AUDIO



