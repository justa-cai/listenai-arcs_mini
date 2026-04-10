/*
 *
 *
 */

#include "cli_main.h"

#include <stdarg.h>
#include <assert.h>
#include "chip.h"
#include "IOMuxManager.h"
#include "ClockManager.h" 
#include "Driver_GPIO.h"
#include "Driver_ADC_PDM.h"
#include "Driver_DAC.h"
#include <math.h>
#include "cache.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif


#define DMA_CH_AUD_TX_DEF           ((uint8_t) 2)   // CLASSD/I2S TX
#define DMA_CH_AUD_RX_DEF           ((uint8_t) 1)   // RX LEFT CHANNEL
#define DMA_CH_AUD_RX_R_DEF         ((uint8_t) 3)   // RX RIGHT CHANNEL

int32_t *dac_buf = NULL;
uint32_t *adc_buf = NULL;
float *fft_input = NULL;
float *fft_output = NULL;
float *fft_mag = NULL;

int32_t delay = 0;

#define USE_16BITS      0 // 0:32bit
#define SAMPLE_RATE     8000 //16000 //48000 //

void *g_adc0 = NULL;
void *g_dac0 = NULL;

uint32_t *adc_ping_buf = NULL;
uint32_t *adc_pong_buf = NULL;
int32_t *dac_ping_buf = NULL;
int32_t *dac_pong_buf = NULL;

volatile uint8_t adc_ping_done = 0;
volatile uint8_t adc_pong_done = 0;
volatile uint8_t dac_ping_done = 0;
volatile uint8_t dac_pong_done = 0;


/******************************** DAC data send ********************************/
static uint32_t* s_aud_write_buf [2];

volatile int g_playdone = 0;

void cb_dac01_out_event(uint32_t event_info, uint32_t usr_param)
{
    void *dac_grp = (void*)usr_param;
    uint16_t event;
    uint8_t ch_no;
    int32_t ret;

    event = event_info & CSK_DAC_EVENT_MASK;
    ch_no = (event_info >> CSK_DAC_EVENT_CH_POS) & 0x3;

    if (event & CSK_DAC_EVENT_TX_PING_DONE) {
        dac_ping_done = 1;
    }

    if (event & CSK_DAC_EVENT_TX_PONG_DONE) {
        dac_pong_done = 1;
    }

    if(event & CSK_DAC_EVENT_SEND_COMPLETE){
        g_playdone = 1;
        int32_t rx_cnt = DAC_GetTxCount(dac_grp, DAC_BMP_LEFT);
        CLI_LOG("%s: DAC (ch_no = %d, rx = %d) Play DONE!!\n",
                __func__, ch_no, rx_cnt);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        DAC_Abort(dac_grp, DAC_BMP_LEFT, 0);
        CLI_LOG("%s: DAC TX FIFO Underflow!!\r\n", __func__);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        CLI_LOG("%s: DAC TX FIFO Empty!!\r\n", __func__);
    }
}

void dac_data_init(int32_t *data, size_t num_samples, float freq)
{
    float amplitude = 2147483647.0f * 0.5f;

    for(int i = 0; i < num_samples; i++) {
        data[i] = (int32_t)(amplitude * sinf(2.0f * M_PI * freq * i / SAMPLE_RATE));
    }
    s_aud_write_buf[0] = (uint32_t *) data;
    s_aud_write_buf[1] = (uint32_t *) data;
}


void * init_dac_out(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, uint32_t echo_sampling_freq, CSK_DAC_SignalEvent_t cb_event)
{
    void *dac_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLI_LOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check sample rate
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_DAC_SR_8KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
    case 16000:
        SR_reg_val = CSK_DAC_SR_16KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
    case 24000:
        SR_reg_val = CSK_DAC_SR_24KHZ;
        OSR_reg_val = CSK_DAC_OSR_250;
        break;
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
    default:
        CLI_LOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get DAC device group
    dac_grp = DAC01();
    if (dac_grp == NULL)
        return NULL;

    DAC_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_out_left = DMA_CH_AUD_TX_DEF;

    uint8_t dev_bmp_flag = dev_bmp << DAC_BMP_FLAG_OUT_POS;
    if (use_16bits) dev_bmp_flag |= DAC_BMP_FLAG_USE_16BITS;
    iret = DAC_Initialize(dac_grp, cb_event, (uint32_t)dac_grp, dev_bmp_flag, &dma_chs);

    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = DAC_PowerControl(dac_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    iret = DAC_Control(dac_grp, SR_reg_val | OSR_reg_val | CSK_DAC_SOFT_MUTE_SET, // | tx_config
                    CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(0x3)); // arg bit[4:0] = soft mute setting
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;


    // Volume settings for Left DAC device (for only 1 DAC on ARCS!)
    uint32_t gain_a=0, gain_d=0, vol_flag=0;
    if (dev_bmp & DAC_BMP_LEFT) {
        gain_a |= DAC_GAIN_A_VAL(0); // 0 dB
        gain_d |= DAC_GAIN_D_VAL(0); // 0 dB
        //gain_a |= DAC_GAIN_A_VAL(-6); // -6 dB
        //gain_d |= DAC_GAIN_D_VAL(-10); // -10 dB
        vol_flag |= (DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT);
    }

    iret = DAC_SetVolume(dac_grp, gain_a, gain_d, vol_flag);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Mute settings for L/R DAC device
    uint8_t mute_val = 0;
    if (dev_bmp & DAC_BMP_LEFT) {
        //mute_val = DAC_BMP_LEFT;
        mute_val &= ~DAC_BMP_LEFT;
    }

    iret = DAC_SetMute(dac_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    CLI_LOGD("%s: DAC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        DAC_Uninitialize(dac_grp);
        return NULL;
    }

    return dac_grp;
}

#define CHANNEL_BITS        32 //16 //

void test_dac_out(int32_t *data, size_t num)
{
    int32_t ret;

    dac_data_init(data, num, 1000.0f);

    g_dac0 = init_dac_out(DAC_BMP_LEFT, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, SAMPLE_RATE, (CSK_DAC_SignalEvent_t)cb_dac01_out_event);
    if (g_dac0 == NULL)
        return;

    ret = DAC_Send(g_dac0, (const uint32_t *)data, num, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);
    

    while (1) {
        CLI_LOGD("DAC PLAY\n");
        while(g_playdone == 0);
        g_playdone = 0;
        ret = DAC_Send(g_dac0, (const uint32_t *)data, num, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
        //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);
    }
    return;
}


#define OUT_BLK_CNT         2
int32_t test_dac_out_pipo(int32_t *tx_buf, size_t tx_len, float freq)
{
    int32_t ret;
    uint32_t *out_ptr = (uint32_t *)tx_buf;
    uint32_t out_bytes = tx_len * 4;
    uint8_t i;

    PIPO_OUT_BLOCK s_out_blks[OUT_BLK_CNT] = { 0 };


    for (i=0; i<OUT_BLK_CNT; i++) {
        s_out_blks[i].sample_data = out_ptr;
        s_out_blks[i].sample_cnt = out_bytes / 4;
    }


    dac_data_init(tx_buf, tx_len, freq);

    g_dac0 = init_dac_out(DAC_BMP_LEFT, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, SAMPLE_RATE, (CSK_DAC_SignalEvent_t)cb_dac01_out_event);

    uint8_t blk_cnt = i;
    ret = DAC_Send_PiPo(g_dac0, s_out_blks, &blk_cnt, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);

    return ret;
}


/******************************** ADC data collection ********************************/

static volatile uint8_t adc0_flag = 0;
static volatile uint8_t adc1_flag = 0;

void cb_adc0_event(uint32_t event_info, uint32_t usr_param)
{
    void *adc_grp = (void*)usr_param;
    uint32_t rxd_len;
    uint8_t event, ch_no;

    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;
    if (ch_no != 0 && ch_no != 1) {
        CLI_LOGE("adc channel no is not right \n");
        return;
    }


    if(event & CSK_ADCPDM_EVENT_RECEIVE_COMPLETE){

        rxd_len = ADC_PDM_GetRxCount(adc_grp, (0x1 << ch_no)) * (USE_16BITS ? 2 : 4);
        CLI_LOGI("%s: [Done] receive %dB from DIN pin\r\n", __func__, rxd_len);
        // TODO: add your handler here...
        // just for one packet
        if (ch_no == 0) {
            adc0_flag = 1;
            ADC_PDM_Disable(adc_grp, CH_BMP_LEFT);
        } else {
            adc1_flag = 1;
            ADC_PDM_Disable(adc_grp, CH_BMP_RIGHT);
        }
#if 0
        //read repeatedly
        int32_t ret;
        if (ch_no == 0) // left channel
            ret = ADC_PDM_Receive(g_adc0, s_aud_buf_0, s_samp_cnt, CH_BMP_LEFT, ADC_PDM_RX_FLAG_START_NOW);
        else // right channel
            ret = ADC_PDM_Receive(g_adc0, s_aud_buf_1, s_samp_cnt, CH_BMP_RIGHT, ADC_PDM_RX_FLAG_START_NOW);
        if (ret != CSK_DRIVER_OK) {
            CLOGW("%s: read channel left/right failed!", __func__);
        }
#endif
    }
    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) {
        CLI_LOGW("%s: ADC/DMIC01 RX FIFO Overflow!!\r\n", __func__);
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) {
        CLI_LOGW("%s: ADC/DMIC01 RX FIFO Full!!\r\n", __func__);
    }

    if( event & CSK_ADCPDM_EVENT_RX_PING_DONE) {
        adc_ping_done = 1;
    }

    if( event & CSK_ADCPDM_EVENT_RX_PONG_DONE) {
        adc_pong_done = 1;
    }

}


void * init_adc_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event)
{
    void *adc_grp;
    int32_t iret, SR_reg_val, OSR_reg_val;

    // Check the channel count
    if (dev_bmp == 0) {
        CLI_LOGE("%s: NO channel is set!\r\n", __func__);
        return NULL;
    }

    // Check sample rate, SR * OSR = 4M or 12M is required!!
    switch (sampling_freq) {
    case 8000:
        SR_reg_val = CSK_ADCPDM_SR_8KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_500; // CSK_ADCPDM_OSR_250
        break;
    case 16000:
        SR_reg_val = CSK_ADCPDM_SR_16KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_250; // CSK_ADCPDM_OSR_125
        break;
    case 48000:
        SR_reg_val = CSK_ADCPDM_SR_48KHZ;
        OSR_reg_val = CSK_ADCPDM_OSR_250; // CSK_ADCPDM_OSR_50
        break;
    default:
        CLI_LOGE("%s: CANNOT support sample rate: %d\n", __func__, sampling_freq);
        return NULL;
    }

    // Get ADC device group
    adc_grp = ADC_PDM01();
    //adc_grp = ADC_PDM23();
    if (adc_grp == NULL)
        return NULL;

    ADC_PDM_DMA_CHS dma_chs;
    memset(&dma_chs, 0xFF, sizeof(dma_chs));
    dma_chs.dma_ch_in_left = DMA_CH_AUD_RX_DEF;
    dma_chs.dma_ch_in_right = DMA_CH_AUD_RX_R_DEF;

    //IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_12 = 1;
    uint8_t dev_bmp_flag = dev_bmp << ADC_PDM_BMP_FLAG_IN_POS; // ADC_PDM_USE_PDM
    if (use_16bits) dev_bmp_flag |= ADC_PDM_BMP_FLAG_USE_16BITS;
    iret = ADC_PDM_Initialize(adc_grp, cb_event, (uint32_t)adc_grp, dev_bmp_flag, &dma_chs);
    if (iret != CSK_DRIVER_OK)
        return NULL;

    iret = ADC_PDM_PowerControl(adc_grp, CSK_POWER_FULL);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    uint32_t control = SR_reg_val | OSR_reg_val | CSK_ADCPDM_RXCFG_MIXED | CSK_ADCPDM_HPF_SET;
    uint32_t arg = CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3);

#if ADC_SINGLE_INPUT
    control |= CSK_ADCPDM_PGA_INPUT_SET;
    arg |= CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE | CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE;
#endif

    iret = ADC_PDM_Control(adc_grp, control, arg);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Volume settings for L/R ADC devices
    uint32_t gain_a=0, gain_d=0, vol_flag=0;
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        gain_a |= ADC_PDM_GAIN_A_VAL(0); // 0 dB (0x6)
        gain_d |= ADC_PDM_GAIN_D_VAL(0); // 0 dB (0x55)
        //gain_a |= ADC_PDM_GAIN_A_VAL(1); // 1 dB
        //gain_d |= ADC_PDM_GAIN_D_VAL(18); // 30, 25, 20, 15, 5 dB
        vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;
    }
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        gain_a |= ADC_PDM_GAIN_A_VAL(0) << 16; // 0 dB (0x6 << 16)
        gain_d |= ADC_PDM_GAIN_D_VAL(0) << 16; // 0 dB (0x55 << 16)
        //gain_a |= ADC_PDM_GAIN_A_VAL(1) << 16; // 1 dB
        //gain_d |= ADC_PDM_GAIN_D_VAL(18) << 16; // 30, 25, 20, 15, 5 dB
        vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;
    }
    iret = ADC_PDM_SetVolume(adc_grp, gain_a, gain_d, vol_flag);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // Mute settings for L/R ADC device
    uint8_t mute_val = 0;
    if (dev_bmp & ADC_PDM_BMP_LEFT) {
        mute_val &= ~ADC_PDM_BMP_LEFT;
    }
    if (dev_bmp & ADC_PDM_BMP_RIGHT) {
        mute_val &= ~ADC_PDM_BMP_RIGHT;
    }
    iret = ADC_PDM_SetMute(adc_grp, mute_val, dev_bmp);
    if (iret != CSK_DRIVER_OK)
        goto ERR_EXIT;

    // for set mic pin
    //iomux pin
    // for set mic pin
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_REG = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_REG = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_UP = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_REG = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_REG = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_UP = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_REG = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_REG = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_UP = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_DN = 0x0; // 1 bits

    IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_REG = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_REG = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_FRC = 0x1; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_UP = 0x0; // 1 bits
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_DN = 0x0; // 1 bits

   	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 21; // MIC0_INP
   	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 21; // MIC0_INN
   	IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 21; // MIC1_INP
   	IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 21; // MIC1_INN

    CLI_LOGD("%s: ADC module is initialized successfully!!\r\n", __func__);

ERR_EXIT:
    if (iret != CSK_DRIVER_OK) {
        ADC_PDM_Uninitialize(adc_grp);
        return NULL;
    }

    return adc_grp;
}


void simple_fft(float *real, float *imag, int n)
{
    int i, j, k, m;
    float t_real, t_imag, u_real, u_imag, w_real, w_imag;

    // Bit-reversal permutation
    j = 0;
    for (i = 0; i < n - 1; i++) {
        if (i < j) {
            t_real = real[j]; real[j] = real[i]; real[i] = t_real;
            t_imag = imag[j]; imag[j] = imag[i]; imag[i] = t_imag;
        }
        k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    // Butterfly
    for (m = 1; m < n; m *= 2) {
        u_real = 1.0f;
        u_imag = 0.0f;
        w_real = cosf(M_PI / m);
        w_imag = -sinf(M_PI / m);

        for (j = 0; j < m; j++) {
            for (i = j; i < n; i += 2 * m) {
                int ip = i + m;
                t_real = u_real * real[ip] - u_imag * imag[ip];
                t_imag = u_real * imag[ip] + u_imag * real[ip];

                real[ip] = real[i] - t_real;
                imag[ip] = imag[i] - t_imag;
                real[i] = real[i] + t_real;
                imag[i] = imag[i] + t_imag;
            }
            float tmp_real = u_real * w_real - u_imag * w_imag;
            u_imag = u_real * w_imag + u_imag * w_real;
            u_real = tmp_real;
        }
    }
}

int32_t test_adc_in(uint32_t channel_bmp, uint32_t *rec_buf, int size)
{
    int32_t ret;

    g_adc0 = init_adc_in(channel_bmp, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, (CSK_ADC_PDM_SignalEvent_t)cb_adc0_event);

    ret = ADC_PDM_Receive(g_adc0, rec_buf, size, channel_bmp, ADC_PDM_RX_FLAG_START_NOW);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);
    if (ret)
        return ret;

    // CLI_LOGD("Wait for ADC receive\n");
    while(adc0_flag == 0 && adc1_flag == 0);
    adc0_flag = 0;
    adc1_flag = 0;
    // CLI_LOGD("ADC receive done\n");
    if (!delay)
        rtos_delay(1000);
    else
        rtos_delay(delay);

    ret = ADC_PDM_Receive(g_adc0, rec_buf, size, channel_bmp, ADC_PDM_RX_FLAG_START_NOW);
    //TEST_ASSERT_EQUAL_INT32(CSK_DRIVER_OK, ret);
    if (ret)
        return ret;

    CLI_LOGD("Wait for ADC receive\n");
    while(adc0_flag == 0 && adc1_flag == 0);
    adc0_flag = 0;
    adc1_flag = 0;
    CLI_LOGD("ADC receive done\n");
    return ret;
}

#define FFT_SIZE       512  //1024 // 512 //

static void test_dac2adc(uint8_t adc_chan)
{
    // User defined ADC channel 
    uint32_t adc_ch_bmp;

    if (adc_chan)
        adc_ch_bmp = ADC_PDM_BMP_RIGHT;
    else 
        adc_ch_bmp = ADC_PDM_BMP_LEFT;

    dac_buf = (int32_t *)rtos_malloc(FFT_SIZE * sizeof(int32_t));
    adc_buf = (uint32_t *)rtos_malloc(FFT_SIZE * sizeof(uint32_t));
    fft_input = (float *)rtos_malloc(FFT_SIZE * sizeof(float));
    fft_output = (float *)rtos_malloc(FFT_SIZE * sizeof(float));
    fft_mag = (float *)rtos_malloc((FFT_SIZE / 2) * sizeof(float));

    if (!dac_buf || !adc_buf || !fft_input || !fft_output || !fft_mag) {
        CLI_LOG("MALLOC FAIL \n");
        goto END;
    }
    memset(dac_buf, 0, FFT_SIZE * sizeof(int32_t));
    memset(adc_buf, 0, FFT_SIZE * sizeof(uint32_t));

    // Get ADC device group
    g_adc0 = ADC_PDM01();
    // Get DAC device group
    g_dac0 = DAC01();

    //TEST_ASSERT(g_dac0 != NULL);
    //TEST_ASSERT(g_adc0 != NULL);
    if (!g_dac0 || !g_adc0) {
        CLI_LOG("Get dac or adc fail \n");
        goto END;
    }

    float target_freq = 1000.0f;
    float measured_freq;

    // test_dac_out(dac_buf, FFT_SIZE);
    test_dac_out_pipo(dac_buf, FFT_SIZE, target_freq);
    if (!delay)
        rtos_delay(1000);
    else
        rtos_delay(delay);
    test_adc_in(adc_ch_bmp, adc_buf, FFT_SIZE);

    // Uninitialize DAC and ADC
    DAC_Uninitialize(g_dac0);
    ADC_PDM_Uninitialize(g_adc0); 

    // Convert to float
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_input[i] = (float)((int32_t)adc_buf[i]);
        fft_output[i] = 0.0f; // Use fft_output as imaginary part
    }

#if 0 // Use NMSIS-DSP library

    // FFT Calculation
    riscv_rfft_fast_instance_f32 S;
    riscv_rfft_fast_init_f32(&S, FFT_SIZE);
    riscv_rfft_fast_f32(&S, fft_input, fft_output, 0);
    riscv_cmplx_mag_f32(fft_output, fft_mag, FFT_SIZE / 2);

    float32_t maxValue;
    uint32_t maxIndex;
    riscv_max_f32(&fft_mag[1], FFT_SIZE / 2 - 1, &maxValue, &maxIndex);
    maxIndex++;

#else

    // FFT Calculation
    simple_fft(fft_input, fft_output, FFT_SIZE);
    
    // Magnitude
    for(int i=0; i < FFT_SIZE / 2; i++) {
        fft_mag[i] = sqrtf(fft_input[i]*fft_input[i] + fft_output[i]*fft_output[i]);
    }

    float maxValue = 0.0f;
    uint32_t maxIndex = 0;
    
    // Skip DC (index 0)
    for (uint32_t i = 1; i < FFT_SIZE / 2; i++) {
        if (fft_mag[i] > maxValue) {
            maxValue = fft_mag[i];
            maxIndex = i;
        }
    }
#endif

    measured_freq = (float)maxIndex * SAMPLE_RATE / (float)FFT_SIZE;
    CLI_LOG("\r\nDominant Frequency: %.2f Hz\r\n", measured_freq);

    //TEST_ASSERT_FLOAT_WITHIN(10.0f, target_freq, measured_freq);
    /*if ((measured_freq < target_freq) && (measured_freq + 10.0f >= target_freq) || \
        (measured_freq > target_freq) && (target_freq + 10.0f >= measured_freq))*/
    if (fabs(measured_freq - target_freq) <= 10.0f)
        CLI_LOG("PASS: ADC/DAC test success\n");
    else
        CLI_LOG("FAIL: ADC/DAC test fail\n");
END:
    if (dac_buf) { rtos_free(dac_buf); dac_buf = NULL; }
    if (adc_buf) { rtos_free(adc_buf); adc_buf = NULL; }
    if (fft_input) { rtos_free(fft_input); fft_input = NULL; }
    if (fft_output) { rtos_free(fft_output); fft_output = NULL; }
    if (fft_mag) { rtos_free(fft_mag); fft_mag = NULL; }

    return;
}

int cli_dac2adc_test(char *params)
{
    char *next = params, *token = NULL, *token_val = NULL;
    uint8_t adc_channel = 0;
    int res = CLI_SUCCESS;

    #ifdef WIFI_RAM_ATE
    disable_GINT();
    HAL_FlushInvalidateDCache();
    HAL_DisableDCache();
    enable_GINT();
    #endif

    token = utils_next_token(&next);
    if(!token) {
        res = CLI_SHOW_USAGE;
        return res;
    }

    adc_channel = atoi(token);
    if (adc_channel != 0 && adc_channel != 1) {
        res = CLI_SHOW_USAGE;
        return res;
    }

    token = utils_next_token(&next);
    if (token)
        delay = atoi(token);
    else
        delay = 0;

    test_dac2adc(adc_channel);

    #ifdef WIFI_RAM_ATE
    disable_GINT();
    HAL_EnableDCache();
    enable_GINT();
    #endif

    return res;
}


#define PACKET_SIZE       512 //1024 // 512 //
#define FIFO_BLOCKS 3

void test_adc2dac(uint8_t adc_chan)
{
    // User defined ADC channel
    uint32_t adc_ch_bmp;
    uint32_t *fifo[FIFO_BLOCKS];
    int fifo_write_idx = 0;
    int fifo_read_idx = 0;
    int fifo_count = 0;
    PIPO_IN_BLOCK s_in_blks[2] = { 0 };
    PIPO_OUT_BLOCK s_out_blks[2] = { 0 };
    uint8_t in_blk_cnt = 2;
    uint8_t out_blk_cnt = 2;

    int32_t ret;

    if (adc_chan)
        adc_ch_bmp = ADC_PDM_BMP_RIGHT;
    else
        adc_ch_bmp = ADC_PDM_BMP_LEFT;

    adc_ping_buf = (uint32_t *)rtos_malloc(PACKET_SIZE * sizeof(uint32_t));
    adc_pong_buf = (uint32_t *)rtos_malloc(PACKET_SIZE * sizeof(uint32_t));
    dac_ping_buf = (int32_t *)rtos_malloc(PACKET_SIZE * sizeof(int32_t));
    dac_pong_buf = (int32_t *)rtos_malloc(PACKET_SIZE * sizeof(int32_t));

    if (!adc_ping_buf || !adc_pong_buf || !dac_ping_buf || !dac_pong_buf)
    {
        CLOGE("MALLOC FAIL \n");
        goto END;
	}

    for (int i = 0; i < FIFO_BLOCKS; i++) {
        fifo[i] = (uint32_t *)rtos_malloc(PACKET_SIZE * sizeof(uint32_t));
        if (!fifo[i]) {
            CLOGE("FIFO MALLOC FAIL \n");
            goto END;
		}
        memset(fifo[i], 0, PACKET_SIZE * sizeof(uint32_t));
    }

    memset(adc_ping_buf, 0, PACKET_SIZE * sizeof(uint32_t));
    memset(adc_pong_buf, 0, PACKET_SIZE * sizeof(uint32_t));
    memset(dac_ping_buf, 0, PACKET_SIZE * sizeof(int32_t));
    memset(dac_pong_buf, 0, PACKET_SIZE * sizeof(int32_t));

    g_adc0 = init_adc_in(adc_ch_bmp, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, (CSK_ADC_PDM_SignalEvent_t)cb_adc0_event);
    g_dac0 = init_dac_out(DAC_BMP_LEFT, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, SAMPLE_RATE, (CSK_DAC_SignalEvent_t)cb_dac01_out_event);

    if (!g_dac0 || !g_adc0) {
        CLI_LOG("Get dac or adc fail \n");
        goto END;
    }


    CLI_LOGI("ADC and DAC initialized (rate=%d Hz, packet=%d samples)", SAMPLE_RATE, PACKET_SIZE);

    s_in_blks[0].sample_data = adc_ping_buf;
    s_in_blks[0].sample_cnt = PACKET_SIZE;
    s_in_blks[1].sample_data = adc_pong_buf;
    s_in_blks[1].sample_cnt = PACKET_SIZE;

    s_out_blks[0].sample_data = (uint32_t *)dac_ping_buf;
    s_out_blks[0].sample_cnt = PACKET_SIZE;
    s_out_blks[1].sample_data = (uint32_t *)dac_pong_buf;
    s_out_blks[1].sample_cnt = PACKET_SIZE;


    adc_ping_done = 0;
    adc_pong_done = 0;
    dac_ping_done = 0;
    dac_pong_done = 0;

    ret = ADC_PDM_Receive_PiPo(g_adc0, s_in_blks, &in_blk_cnt, adc_ch_bmp, ADC_PDM_RX_FLAG_START_NOW);
	if (ret) {
        CLI_LOGE("ADC_PDM_Receive_PiPo fail %d\n", ret);
        goto END;
	}

    // Wait for FIFO to fill with at least 2 blocks before starting DAC
    CLI_LOGI("FIFO pre-fill started, waiting for 2 blocks...");
    while(fifo_count < 2) {
        if (adc_ping_done) {
            adc_ping_done = 0;
            memcpy(fifo[fifo_write_idx], adc_ping_buf, PACKET_SIZE * sizeof(uint32_t));
            fifo_write_idx = (fifo_write_idx + 1) % FIFO_BLOCKS;
            fifo_count++;
            CLI_LOGI("[ADC] PING -> FIFO, count=%d", fifo_count);
        }
        if (adc_pong_done) {
            adc_pong_done = 0;
            memcpy(fifo[fifo_write_idx], adc_pong_buf, PACKET_SIZE * sizeof(uint32_t));
            fifo_write_idx = (fifo_write_idx + 1) % FIFO_BLOCKS;
            fifo_count++;
            CLI_LOGI("[ADC] PONG -> FIFO, count=%d", fifo_count);
        }
    }

    // Pre-fill DAC buffers from FIFO
    memcpy(dac_ping_buf, fifo[fifo_read_idx], PACKET_SIZE * sizeof(uint32_t));
    fifo_read_idx = (fifo_read_idx + 1) % FIFO_BLOCKS;
    fifo_count--;

    memcpy(dac_pong_buf, fifo[fifo_read_idx], PACKET_SIZE * sizeof(uint32_t));
    fifo_read_idx = (fifo_read_idx + 1) % FIFO_BLOCKS;
    fifo_count--;

    ret = DAC_Send_PiPo(g_dac0, s_out_blks, &out_blk_cnt, DAC_BMP_LEFT, DAC_TX_FLAG_START_NOW);
	if (ret) {
        CLI_LOGE("DAC_Send_PiPo fail %d\n", ret);
        goto END;
	}

    CLI_LOGD("ADC to DAC loop started with FIFO\n");

    while(1) {
        if (adc_ping_done) {
            adc_ping_done = 0;
            if (fifo_count < FIFO_BLOCKS) {
                int stored_idx = fifo_write_idx;
                memcpy(fifo[fifo_write_idx], adc_ping_buf, PACKET_SIZE * sizeof(uint32_t));
                fifo_write_idx = (fifo_write_idx + 1) % FIFO_BLOCKS;
                fifo_count++;
                CLI_LOGI("[ADC] PING -> FIFO[%d], count=%d", stored_idx, fifo_count);
            } else {
                CLI_LOGW("[ADC] FIFO Overflow! PING dropped, count=%d", fifo_count);
            }
        }
        if (adc_pong_done) {
            adc_pong_done = 0;
            if (fifo_count < FIFO_BLOCKS) {
                int stored_idx = fifo_write_idx;
                memcpy(fifo[fifo_write_idx], adc_pong_buf, PACKET_SIZE * sizeof(uint32_t));
                fifo_write_idx = (fifo_write_idx + 1) % FIFO_BLOCKS;
                fifo_count++;
                CLI_LOGI("[ADC] PONG -> FIFO[%d], count=%d", stored_idx, fifo_count);
            } else {
                CLI_LOGW("[ADC] FIFO Overflow! PONG dropped, count=%d", fifo_count);
            }
        }
        if (dac_ping_done) {
            dac_ping_done = 0;
            if (fifo_count > 0) {
                int read_idx = fifo_read_idx;
                memcpy(dac_ping_buf, fifo[fifo_read_idx], PACKET_SIZE * sizeof(uint32_t));
                fifo_read_idx = (fifo_read_idx + 1) % FIFO_BLOCKS;
                fifo_count--;
                CLI_LOGI("[DAC] PING <- FIFO[%d], count=%d", read_idx, fifo_count);
            } else {
                CLI_LOGW("[DAC] FIFO Underflow! PING muted, count=%d", fifo_count);
                memset(dac_ping_buf, 0, PACKET_SIZE * sizeof(uint32_t));
            }
        }
        if (dac_pong_done) {
            dac_pong_done = 0;
            if (fifo_count > 0) {
                int read_idx = fifo_read_idx;
                memcpy(dac_pong_buf, fifo[fifo_read_idx], PACKET_SIZE * sizeof(uint32_t));
                fifo_read_idx = (fifo_read_idx + 1) % FIFO_BLOCKS;
                fifo_count--;
                CLI_LOGI("[DAC] PONG <- FIFO[%d], count=%d", read_idx, fifo_count);
            } else {
                CLI_LOGW("[DAC] FIFO Underflow! PONG muted, count=%d", fifo_count);
                memset(dac_pong_buf, 0, PACKET_SIZE * sizeof(uint32_t));
            }
        }
    }

    // Uninitialize DAC and ADC
    DAC_Uninitialize(g_dac0);
    ADC_PDM_Uninitialize(g_adc0);

END:
    if (adc_ping_buf) { rtos_free(adc_ping_buf); adc_ping_buf = NULL; }
    if (adc_pong_buf) { rtos_free(adc_pong_buf); adc_pong_buf = NULL; }
    if (dac_ping_buf) { rtos_free(dac_ping_buf); dac_ping_buf = NULL; }
    if (dac_pong_buf) { rtos_free(dac_pong_buf); dac_pong_buf = NULL; }
    for (int i = 0; i < FIFO_BLOCKS; i++) {
        if (fifo[i]) { rtos_free(fifo[i]); fifo[i] = NULL; }
    }

    return;
}


int cli_adc2dac_test(char *params)
{
    char *next = params, *token = NULL, *token_val = NULL;
    uint8_t adc_channel = 0;
    int res = CLI_SUCCESS;

    #ifdef WIFI_RAM_ATE
    disable_GINT();
    HAL_FlushInvalidateDCache();
    HAL_DisableDCache();
    enable_GINT();
    #endif

    token = utils_next_token(&next);
    if(!token) {
        res = CLI_SHOW_USAGE;
        return res;
    }

    adc_channel = atoi(token);
    if (adc_channel != 0 && adc_channel != 1) {
        res = CLI_SHOW_USAGE;
        return res;
    }

    test_adc2dac(adc_channel);

    #ifdef WIFI_RAM_ATE
    disable_GINT();
    HAL_EnableDCache();
    enable_GINT();
    #endif

    return res;
}

