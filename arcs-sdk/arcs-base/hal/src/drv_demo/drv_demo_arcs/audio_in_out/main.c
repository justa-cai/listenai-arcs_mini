/*
 * main.c
 *
 *  Created on: Nov.5, 2020 for VENUS (CP)
 *  Ported on: Dec. 18, 2023 for ARCS (AP)
 *
 */

#include "main.h"

#include "ClockManager.h"

#include "audio_clip1.h"
#include "earthquake_16k_16bit_double.h"
#include "earthquake_16k_16bit_single.h"

#define DEBUG_LOG   1 //0 //
#if DEBUG_LOG
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGD(format, ...)   CLOG(format, ##__VA_ARGS__)
#define HEXP                hexPrint
#else
#define LOGD(format, ...)   ((void)0)
#define HEXP(...)           ((void)0)
#endif // DEBUG_LOG

// audio in/out test type definitions
#define AIOT_ADC_IN_I2S_OUT     0
#define AIOT_DMIC_IN_I2S_OUT    1
#define AIOT_I2S_IN_DAC_OUT     2
#define AIOT_ADC_IN_DAC_OUT     3
#define AIOT_I2S_IN_I2S_OUT     4

#define AIOT_ONESHOT_ADC_IN     5
#define AIOT_ONESHOT_DMIC_IN    6
#define AIOT_ONESHOT_I2S_IN     7
#define AIOT_ONESHOT_DAC_OUT    8
#define AIOT_ONESHOT_I2S_OUT    9

// use PingPong API or not?
#define NOT_USE_PIPO_API    0 //1 //

// current audio in/out test type
#define CUR_AIOT     AIOT_ADC_IN_I2S_OUT    //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_DMIC_IN_I2S_OUT //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_I2S_IN_DAC_OUT  //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_ADC_IN_DAC_OUT  //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_I2S_IN_I2S_OUT  //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_ONESHOT_ADC_IN  //PIPO Code Done! Verified!
//#define CUR_AIOT     AIOT_ONESHOT_DMIC_IN //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_ONESHOT_I2S_IN  //PIPO Code Done! Verified?
//#define CUR_AIOT     AIOT_ONESHOT_DAC_OUT //PIPO Code Done! Verified!
//#define CUR_AIOT     AIOT_ONESHOT_I2S_OUT //PIPO Code Done! Verified?


#if (CUR_AIOT == AIOT_ADC_IN_I2S_OUT)
#define EXT_CODEC       0
#define CHANNEL_COUNT   2
#define I2S_OUT_BMP     CH_BMP_STEREO //CH_BMP_LEFT //CH_BMP_RIGHT
#define ADC_BMP         ADC_PDM_BMP_STEREO //ADC_PDM_BMP_LEFT //ADC_PDM_BMP_RIGHT

#elif (CUR_AIOT == AIOT_I2S_IN_DAC_OUT)
#define EXT_CODEC       0
#define CHANNEL_COUNT   1
#define I2S_IN_BMP      CH_BMP_LEFT //CH_BMP_STEREO //CH_BMP_RIGHT
#define DAC_BMP         DAC_BMP_LEFT //DAC_BMP_STEREO

#elif (CUR_AIOT == AIOT_ADC_IN_DAC_OUT)
#define EXT_CODEC       0
#define CHANNEL_COUNT   1
#define ADC_BMP         ADC_PDM_BMP_LEFT //ADC_PDM_BMP_STEREO //ADC_PDM_BMP_RIGHT
#define DAC_BMP         DAC_BMP_LEFT //DAC_BMP_STEREO

#elif (CUR_AIOT == AIOT_ONESHOT_ADC_IN)
#define EXT_CODEC       0
#define CHANNEL_COUNT   2
#define ADC_BMP         ADC_PDM_BMP_STEREO //ADC_PDM_BMP_LEFT //ADC_PDM_BMP_RIGHT

#elif (CUR_AIOT == AIOT_ONESHOT_DMIC_IN)
#define EXT_CODEC       0
#define CHANNEL_COUNT   2
#define PDM_BMP         ADC_PDM_BMP_STEREO //ADC_PDM_BMP_LEFT //ADC_PDM_BMP_RIGHT

#elif (CUR_AIOT == AIOT_ONESHOT_DAC_OUT)
#define EXT_CODEC       0
#define CHANNEL_COUNT   1
#define DAC_BMP         DAC_BMP_LEFT //DAC_BMP_STEREO //

#else
#define CHANNEL_COUNT   2 //1 //
#define EXT_CODEC       0 //1 //
#endif

//default general settings
#ifndef I2S_IN_BMP
#define I2S_IN_BMP      CH_BMP_STEREO //CH_BMP_LEFT //CH_BMP_RIGHT //
#endif
#ifndef I2S_OUT_BMP
#define I2S_OUT_BMP     CH_BMP_STEREO //CH_BMP_LEFT //CH_BMP_RIGHT //
#endif
#ifndef ADC_BMP
#define ADC_BMP         ADC_PDM_BMP_STEREO //ADC_PDM_BMP_LEFT //ADC_PDM_BMP_RIGHT
#endif
#ifndef PDM_BMP
#define PDM_BMP         ADC_PDM_BMP_STEREO //ADC_PDM_BMP_LEFT //ADC_PDM_BMP_RIGHT
#endif
#ifndef DAC_BMP
#define DAC_BMP         DAC_BMP_LEFT //DAC_BMP_STEREO // ONLY 1 DAC ON MARS!!
#endif

//------------------------------------------------------------------
#define SAMPLE_RATE         16000 //48000 //

//#define CHANNEL_COUNT       1 //2 //4 //8 //
#define CHANNEL_BITS        16 //32 //

#define GAP_MS              10 // Record is ahead of Play 10 ms
#define STEP_MS             5 // R/W audio data every 5 ms
#define BUF_SIZE_MS         50 // 100 // Ring buffer to hold 100ms audio data at max.

//----------------------------------------------
//#define CHANNEL_BMP         ((0x1UL << CHANNEL_COUNT) - 1)
#define CHANNEL_BYTES       ((CHANNEL_BITS + 7)/8)
#define ONE_MS_BYTES        (SAMPLE_RATE * CHANNEL_COUNT * CHANNEL_BYTES / 1000)
//#define ONE_MS_WORDS        ((ONE_MS_BYTES + 3)/4)

#define GAP_BYTES           (ONE_MS_BYTES * GAP_MS)
#define STEP_BYTES          (ONE_MS_BYTES * STEP_MS) // 320
#define BUF_SIZE_BYTES      (ONE_MS_BYTES * BUF_SIZE_MS) // 6400

static uint32_t s_aud_buf [BUF_SIZE_BYTES / 4];
static uint8_t * volatile s_rp = NULL; // read pointer
static uint8_t * volatile s_wp = NULL; // write pointer
static uint8_t *s_limit = NULL; // audio buffer limit
static volatile bool s_buf_full = false;
static volatile bool s_stop_loop = false;
//------------------------------------------------------------------
void *g_i2c_dev = NULL;
void *g_i2s_in = NULL;
void *g_i2s_out = NULL;
//void *g_dmic_in = NULL;
//void *g_adc_in = NULL;
void *g_adc_dmic_in = NULL;
void *g_dac_out = NULL;
void *gpio_A = NULL;
//void *gpio_B = NULL;

//static CORE_IOMUX_RegDef* iomux_reg = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;

#define IN_BLK_CNT          2
#define IN_RENEW_BLK_CNT    1

#define IN_STEP_BLK_BYTES       STEP_BYTES // (ONE_MS_BYTES * STEP_MS)
#define IN_ECHO_STEP_BLK_BYTES  (IN_STEP_BLK_BYTES * 2) // IN & ECHO data

PIPO_IN_BLOCK s_in_blks[IN_BLK_CNT] = { 0 };

#define OUT_BLK_CNT         2
#define OUT_RENEW_BLK_CNT   1

//#define OUT_BLK_BYTES           (ONE_MS_BYTES * 100) //100ms, in bytes
#define OUT_STEP_BLK_BYTES      STEP_BYTES // (ONE_MS_BYTES * STEP_MS)

PIPO_OUT_BLOCK s_out_blks[OUT_BLK_CNT] = { 0 };

volatile uint32_t s_in_bytes = 0; // in byte
volatile uint32_t s_out_bytes = 0; // in byte

void init_audio_buffer()
{
    memset(&s_aud_buf[0], 0, sizeof(s_aud_buf));
    s_limit = (uint8_t *)s_aud_buf + BUF_SIZE_BYTES;
    s_rp = (uint8_t *)s_aud_buf;
    s_wp = (uint8_t *)s_aud_buf + GAP_BYTES;
}

static void hexPrint(const void *buf, uint32_t len)
{
    int i = 0;
    const uint8_t *data = (const uint8_t *)buf;
    while (i < len) {
        logDbg("%02x ", data[i]);
        i++;
        if (i % 16 == 0) {
            logDbg("\r\n");
        } else if (i % 4 == 0) {
            logDbg(" ");
        }
    }
    logDbg("\r\n\r\n");
}

//------------------------------------------------------------------
//  Continuous record/play functions & callbacks
//------------------------------------------------------------------
bool trigger_i2s_in()
{
//    if (s_buf_full) // audio buffer full
//        return;

    static AUDIO_BUFFER_USER dma_bufs[2];
    int32_t ret, len, len2 = 0;

    len = s_rp - s_wp;
    if (len <= 0) {
        len = s_limit - s_wp;
        len2 = s_rp - (uint8_t*)s_aud_buf;
    }

    // check free size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start receiving audio data
    if (len2 == 0) { // one single buffer
        ret = I2S_Receive(g_i2s_in, (uint32_t*)s_wp, len/CHANNEL_BYTES, I2S_IN_BMP, I2S_RX_FLAG_START_NOW); // 16bit: len/2
    } else { // two buffers LLP
        ret = CSK_DRIVER_ERROR_UNSUPPORTED;
        LOGD("NO I2S_Receive_LLP support!\r\n");
/*
        memset(dma_bufs, 0, sizeof(dma_bufs));
        dma_bufs[0].sample_data = (uint32_t*)s_wp;
        dma_bufs[0].sample_cnt = len/CHANNEL_BYTES; // 16bit: len/2
        dma_bufs[1].sample_data = (uint32_t*)s_aud_buf;
        dma_bufs[1].sample_cnt = len2/CHANNEL_BYTES; // 16bit: len/2
        ret = I2S_Receive_LLP(g_i2s_in, dma_bufs, 2, I2S_IN_BMP, I2S_RX_FLAG_START_NOW);
*/
    }

    return (ret == CSK_DRIVER_OK);
}

bool init_i2s_in_pipo()
{
    int32_t ret;
    uint32_t *in_ptr = (uint32_t *)s_wp;
    uint32_t in_bytes = IN_STEP_BLK_BYTES;
    uint8_t i;

    memset(s_in_blks, 0, sizeof(s_in_blks));
    for (i=0; i<IN_BLK_CNT; i++) {
        s_in_blks[i].sample_data = in_ptr;
        s_in_blks[i].sample_cnt = in_bytes / CHANNEL_BYTES;
        in_ptr += (in_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = I2S_Receive_PiPo(g_i2s_in, s_in_blks, &blk_cnt, I2S_IN_BMP, I2S_RX_FLAG_START_NOW);
    assert(blk_cnt == IN_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
}

bool trigger_i2s_in_pipo()
{
    int32_t ret, len, len2 = 0;

    len = s_rp - s_wp;
    if (len <= 0) {
        len = s_limit - s_wp;
        len2 = s_rp - (uint8_t*)s_aud_buf;
    }

    // check free size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

//    if (len2 != 0) {
//        LOGD("len = %d, len2 = %d\n", len, len2);
//    }
    assert (len2 == 0);

    memset(s_in_blks, 0, sizeof(s_in_blks));
    s_in_blks[0].sample_data = (uint32_t*)s_wp;
    s_in_blks[0].sample_cnt = len / CHANNEL_BYTES;
    s_in_blks[0].flags = 0;

    // start receiving audio data
    uint8_t blk_cnt = 1; //RENEW_BLK_CNT;
    ret = I2S_Receive_PiPo(g_i2s_in, s_in_blks, &blk_cnt, I2S_IN_BMP, I2S_RX_FLAG_START_NOW);
    assert(blk_cnt == 1);
    return (ret == CSK_DRIVER_OK);
}


bool trigger_i2s_out()
{
    static AUDIO_BUFFER_USER dma_bufs[2];
    int32_t ret, len, len2 = 0;

    // get max available audio data for next playback
    len = s_wp - s_rp;
    if (len <= 0) {
        len = s_limit - s_rp;
        len2 = s_wp - (uint8_t*)s_aud_buf;
    }

    // compare with step size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start sending audio data
    if (len2 == 0) { // one single buffer
        ret = I2S_Send(g_i2s_out, (uint32_t*)s_rp, len/CHANNEL_BYTES, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW); // 16bit: len/2
        //ret = I2S_Send(g_i2s_out, (uint32_t*)s_rp, len/CHANNEL_BYTES, CH_BMP_LEFT, I2S_TX_FLAG_START_NOW); // 16bit: len/2
    } else { // two buffers LLP
        ret = CSK_DRIVER_ERROR_UNSUPPORTED;
        LOGD("NO I2S_Send_LLP support!\r\n");
/*
        memset(dma_bufs, 0, sizeof(dma_bufs));
        dma_bufs[0].sample_data = (uint32_t*)s_rp;
        dma_bufs[0].sample_cnt = len/CHANNEL_BYTES; // 16bit: len/2
        dma_bufs[1].sample_data = (uint32_t*)s_aud_buf;
        dma_bufs[1].sample_cnt = len2/CHANNEL_BYTES; // 16bit: len/2
        ret = I2S_Send_LLP(g_i2s_out, dma_bufs, 2, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);
        //ret = I2S_Send_LLP(g_i2s_out, dma_bufs, 2, CH_BMP_LEFT, I2S_TX_FLAG_START_NOW);
*/
    }

    return (ret == CSK_DRIVER_OK);
}

bool init_i2s_out_pipo()
{
    int32_t ret;
    uint32_t *out_ptr = (uint32_t *)s_rp;
    uint32_t out_bytes = OUT_STEP_BLK_BYTES;
    uint8_t i;

    assert((s_wp - s_rp) >= OUT_BLK_CNT * OUT_STEP_BLK_BYTES);
    memset(s_out_blks, 0, sizeof(s_out_blks));
    for (i=0; i<OUT_BLK_CNT; i++) {
        s_out_blks[i].sample_data = out_ptr;
        s_out_blks[i].sample_cnt = out_bytes / CHANNEL_BYTES;
        out_ptr += (out_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = I2S_Send_PiPo(g_i2s_out, s_out_blks, &blk_cnt, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);
    assert(blk_cnt == OUT_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
}

bool trigger_i2s_out_pipo()
{
    int32_t ret, len, len2 = 0;

    // get max available audio data for next playback
    len = s_wp - s_rp;
    if (len <= 0) {
        len = s_limit - s_rp;
        len2 = s_wp - (uint8_t*)s_aud_buf;
    }

    // compare with step size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start receiving audio data
    assert (len2 == 0);

    memset(s_out_blks, 0, sizeof(s_out_blks));
    s_out_blks[0].sample_data = (uint32_t*)s_rp;
    s_out_blks[0].sample_cnt = len / CHANNEL_BYTES;
    s_out_blks[0].flags = 0;

    uint8_t blk_cnt = 1; //RENEW_BLK_CNT;
    ret = I2S_Send_PiPo(g_i2s_out, s_out_blks, &blk_cnt, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);
    assert(blk_cnt == 1);
    return (ret == CSK_DRIVER_OK);
}


bool trigger_adc_dmic_in()
{
//    if (s_buf_full) // audio buffer full
//        return;

    static AUDIO_BUFFER_USER dma_bufs[2];
    int32_t ret, len, len2 = 0;

    len = s_rp - s_wp;
    if (len <= 0) {
        len = s_limit - s_wp;
        len2 = s_rp - (uint8_t*)s_aud_buf;
    }

    // check free size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start receiving audio data
    if (len2 == 0) { // one single buffer
        ret = ADC_PDM_Receive(g_adc_dmic_in, (uint32_t*)s_wp, len/CHANNEL_BYTES, ADC_BMP, ADC_PDM_RX_FLAG_START_NOW); // 16bit: len/2
        //ret = ADC_PDM_Receive(g_adc_dmic_in, (uint32_t*)s_wp, len/CHANNEL_BYTES, PDM_BMP, ADC_PDM_RX_FLAG_START_NOW); // 16bit: len/2
    } else { // two buffers LLP
        ret = CSK_DRIVER_ERROR_UNSUPPORTED;
        LOGD("NO ADC_PDM_Receive support!\r\n");
/*
        memset(dma_bufs, 0, sizeof(dma_bufs));
        dma_bufs[0].sample_data = (uint32_t*)s_wp;
        dma_bufs[0].sample_cnt = len/CHANNEL_BYTES; // 16bit: len/2
        dma_bufs[1].sample_data = (uint32_t*)s_aud_buf;
        dma_bufs[1].sample_cnt = len2/CHANNEL_BYTES; // 16bit: len/2
        ret = ADC_PDM_Receive_LLP(g_adc_dmic_in, dma_bufs, 2, CH_BMP_STEREO, ADC_PDM_RX_FLAG_START_NOW);
*/
    }

    return (ret == CSK_DRIVER_OK);
}

bool init_adc_dmic_in_pipo() // first call of ADC_PDM_Receive_PiPo
{
    int32_t ret;
    uint32_t *in_ptr = (uint32_t *)s_wp;
    uint32_t in_bytes = IN_STEP_BLK_BYTES;
    uint8_t i;

    memset(s_in_blks, 0, sizeof(s_in_blks));
    for (i=0; i<IN_BLK_CNT; i++) {
        s_in_blks[i].sample_data = in_ptr;
        s_in_blks[i].sample_cnt = in_bytes / CHANNEL_BYTES;
        in_ptr += (in_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt, ADC_BMP, ADC_PDM_RX_FLAG_START_NOW);
    //ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt, PDM_BMP, ADC_PDM_RX_FLAG_START_NOW);
    assert(blk_cnt == IN_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
}

bool trigger_adc_dmic_in_pipo() // subsequent calls of ADC_PDM_Receive_PiPo
{
    int32_t ret, len, len2 = 0;

    len = s_rp - s_wp;
    if (len <= 0) {
        len = s_limit - s_wp;
        len2 = s_rp - (uint8_t*)s_aud_buf;
    }

    // check free size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start receiving audio data
    assert (len2 == 0);

    memset(s_in_blks, 0, sizeof(s_in_blks));
    s_in_blks[0].sample_data = (uint32_t*)s_wp;
    s_in_blks[0].sample_cnt = len / CHANNEL_BYTES;
    s_in_blks[0].flags = 0;

    uint8_t blk_cnt = 1; //RENEW_BLK_CNT;
    ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt, ADC_BMP, ADC_PDM_RX_FLAG_START_NOW);
    //ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt, PDM_BMP, ADC_PDM_RX_FLAG_START_NOW);
    assert(blk_cnt == 1);
    return (ret == CSK_DRIVER_OK);
}


bool trigger_dac_out()
{
    static AUDIO_BUFFER_USER dma_bufs[2];
    int32_t ret, len, len2 = 0;

    // get max available audio data for next playback
    len = s_wp - s_rp;
    if (len <= 0) {
        len = s_limit - s_rp;
        len2 = s_wp - (uint8_t*)s_aud_buf;
    }

    // compare with step size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

    // start sending audio data
    if (len2 == 0) { // one single buffer
        ret = DAC_Send(g_dac_out, (uint32_t*)s_rp, len/CHANNEL_BYTES, DAC_BMP, DAC_TX_FLAG_START_NOW); // 16bit: len/2
    } else { // two buffers LLP
        ret = CSK_DRIVER_ERROR_UNSUPPORTED;
        LOGD("NO DAC_Send support!\r\n");
/*
        memset(dma_bufs, 0, sizeof(dma_bufs));
        dma_bufs[0].sample_data = (uint32_t*)s_rp;
        dma_bufs[0].sample_cnt = len/CHANNEL_BYTES; // 16bit: len/2
        dma_bufs[1].sample_data = (uint32_t*)s_aud_buf;
        dma_bufs[1].sample_cnt = len2/CHANNEL_BYTES; // 16bit: len/2
        ret = DAC_Send_LLP(g_dac_out, dma_bufs, 2, DAC_BMP, DAC_TX_FLAG_START_NOW);
*/
    }

    return (ret == CSK_DRIVER_OK);
}

bool init_dac_out_pipo()
{
    int32_t ret;
    uint32_t *out_ptr = (uint32_t *)s_rp;
    uint32_t out_bytes = OUT_STEP_BLK_BYTES;
    uint8_t i;

    assert((s_wp - s_rp) >= OUT_BLK_CNT * OUT_STEP_BLK_BYTES);
    memset(s_out_blks, 0, sizeof(s_out_blks));
    for (i=0; i<OUT_BLK_CNT; i++) {
        s_out_blks[i].sample_data = out_ptr;
        s_out_blks[i].sample_cnt = out_bytes / CHANNEL_BYTES;
        out_ptr += (out_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = DAC_Send_PiPo(g_dac_out, s_out_blks, &blk_cnt, DAC_BMP, DAC_TX_FLAG_START_NOW);
    assert(blk_cnt == OUT_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
}

bool trigger_dac_out_pipo()
{
    int32_t ret, len, len2 = 0;

    // get max available audio data for next playback
    len = s_wp - s_rp;
    if (len <= 0) {
        len = s_limit - s_rp;
        len2 = s_wp - (uint8_t*)s_aud_buf;
    }

    // compare with step size
    if (len >= STEP_BYTES) { // len >= step_size
        len = STEP_BYTES;
        len2 = 0;
    } else if (len > 0) { // len < step_size
        if (len2 >= STEP_BYTES - len)
            len2 = STEP_BYTES - len;
    }

//    if (len2 != 0) {
//        LOGD("len = %d, len2 = %d\n", len, len2);
//    }
    assert (len2 == 0);

    memset(s_out_blks, 0, sizeof(s_out_blks));
    s_out_blks[0].sample_data = (uint32_t*)s_rp;
    s_out_blks[0].sample_cnt = len / CHANNEL_BYTES;
    s_out_blks[0].flags = 0;

    // start receiving audio data
    uint8_t blk_cnt = 1; //RENEW_BLK_CNT;
    ret = DAC_Send_PiPo(g_dac_out, s_out_blks, &blk_cnt, DAC_BMP, DAC_TX_FLAG_START_NOW);
    assert(blk_cnt == 1);
    return (ret == CSK_DRIVER_OK);
}


/**
 \fn          void cb_i2s_in_event (uint32_t event, uint32_t usr_param)
 \brief       Signal I2S Events.
 \param[in]   event_info I2S event and channel information
              bit[7:0] is event type, bit[15:8] is I2S channel number,
              bit[16] indicate the direction, 1 = IN, 0 = OUT
              (NOTE: I2S IN channels and OUT ones are numbered separately)
 \param[in]   usr_param    user parameter
 \return      none
*/
void cb_i2s_in_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint32_t rxd_len;
    uint16_t event;
    uint8_t ch_no, *old_wp;
    bool buf_F = false;

    event = event_info & CSK_I2S_EVENT_MASK;
    ch_no = (event_info >> CSK_I2S_EVENT_I2S_CH_POS) & 0x3;

    if(event & CSK_I2S_EVENT_RX_BLOCK_COMPLETE){
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        memset(s_in_blks, 0, sizeof(s_in_blks));
        int32_t ret = I2S_PiPo_Rxed_Blocks(i2s_dev, s_in_blks, 2, I2S_IN_BMP);
//        LOGD("I2S RX BLK DONE");

        assert(ret == 1);
        rxd_len = s_in_blks[0].sample_cnt * CHANNEL_BYTES;
        //LOGD("I2S RX BLK DONE, rx %d bytes", rxd_len);
        if (rxd_len > 0) {
            // update write pointer
            old_wp = s_wp;
            s_wp += rxd_len;
            if (s_wp >= s_limit)
                s_wp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_wp == s_rp) {
                s_buf_full = true; // audio buffer full
                buf_F = true;
                //LOGD("%s: audio buffer full, no audio data space!!\r\n", __func__);
                //LOGD("buf F\r\n");
            }

            // reuse last WR buffer to avoid recording cutoff
            if (s_buf_full)
                s_wp = old_wp;

            // trigger next i2s record operation
            //if (!s_stop_loop)
            trigger_i2s_in_pipo();

            // print log after trigger next operation
            if (buf_F) {
                LOGD("%s: audio buffer full, no space!!\r\n", __func__);
            }
        } // if (rxd_len > 0)
    }

    if(event & CSK_I2S_EVENT_RECEIVE_COMPLETE){
        rxd_len = I2S_GetRxCount(i2s_dev, (0x1 << ch_no)) * CHANNEL_BYTES;
        //LOGD("%s: [Done] RXD %dB\r\n", __func__, rxd_len);
        if (rxd_len > 0) {
            // update write pointer
            old_wp = s_wp;
            s_wp += rxd_len;
            if (s_wp >= s_limit)
                s_wp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_wp == s_rp) {
                s_buf_full = true; // audio buffer full
                buf_F = true;
                //LOGD("%s: audio buffer full, no audio data space!!\r\n", __func__);
                //LOGD("buf F\r\n");
            }

            // reuse last WR buffer to avoid recording cutoff
            if (s_buf_full)
                s_wp = old_wp;

            // trigger next i2s record operation
            if (!s_stop_loop)
                trigger_i2s_in();

            // print log after trigger next operation
            if (buf_F) {
                LOGD("buf F\r\n");
            }

        } // if (rxd_len > 0)

    }

    if (event & CSK_I2S_EVENT_RX_FIFO_OVERRUN) {
//        s_stop_loop = true;
//        I2S_Abort_Channels(i2s_dev, I2S_IN_BMP, 0, 0);
        CLOGW("%s: I2S RX FIFO Overflow, STOP!!\r\n", __func__);
    }

    if (event & CSK_I2S_EVENT_RX_FIFO_FULL) {
        CLOGW("%s: I2S RX FIFO Full!!\r\n", __func__);
    }
}

void cb_i2s_out_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint32_t txd_len;
    uint16_t event;
    uint8_t ch_no, *old_rp;
    bool buf_E = false;

    event = event_info & CSK_I2S_EVENT_MASK;
    ch_no = (event_info >> CSK_I2S_EVENT_I2S_CH_POS) & 0x3;

    if(event & CSK_I2S_EVENT_TX_BLOCK_COMPLETE){
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        memset(s_out_blks, 0, sizeof(s_out_blks));
        int32_t ret = I2S_PiPo_Txed_Blocks(i2s_dev, s_out_blks, 2, I2S_OUT_BMP);
//        LOGD("I2S TX BLK DONE");

        assert(ret == 1);
        txd_len = s_out_blks[0].sample_cnt * CHANNEL_BYTES;
        //LOGD("I2S TX BLK DONE, tx %d bytes", txd_len);

        if (txd_len > 0) {
            // update write pointer
            old_rp = s_rp;
            s_rp += txd_len;

            if (s_buf_full)
                s_buf_full = false;

            if (s_rp >= s_limit)
                s_rp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_rp == s_wp) {
                // reuse last RD buffer to avoid playback cutoff
                s_rp = old_rp;
                buf_E = true;
                //LOGD("%s: audio buffer empty, no audio data!!\r\n", __func__);
                //LOGD("buf E\r\n");
            }

            // trigger next i2s playback operation
            //if (!s_stop_loop)
            trigger_i2s_out_pipo(); // I2S PingPong TX

            // print log after trigger next operation
            if (buf_E) {
                LOGD("%s: audio buffer empty, no data!!\r\n", __func__);
            }
        } // if (txd_len > 0)
    }

    if(event & CSK_I2S_EVENT_TRANSMIT_COMPLETE){
        txd_len = I2S_GetTxCount(i2s_dev, (0x1 << ch_no)) * CHANNEL_BYTES;
        //LOGD("%s: [Done] TXD %dB\r\n", __func__, txd_len);
        if (txd_len > 0) {
            // update write pointer
            old_rp = s_rp;
            s_rp += txd_len;

            if (s_buf_full)
                s_buf_full = false;

            if (s_rp >= s_limit)
                s_rp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_rp == s_wp) {
                // reuse last RD buffer to avoid playback cutoff
                s_rp = old_rp;
                buf_E = true;
                //LOGD("%s: audio buffer empty, no audio data!!\r\n", __func__);
                //LOGD("buf E\r\n");
            }

            // trigger next i2s playback operation
            if (!s_stop_loop)
                trigger_i2s_out();

            // print log after trigger next operation
            if (buf_E) {
                LOGD("buf E\n");
            }

        } // if (txd_len > 0)

    }

    if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
//        s_stop_loop = true;
//        I2S_Abort_Channels(i2s_dev, 0, I2S_OUT_BMP, 0);
        CLOGW("%s: I2S TX FIFO Underflow, STOP!!\r\n", __func__);
    }

    if (event & CSK_I2S_EVENT_TX_FIFO_EMPTY) {
        CLOGW("%s: I2S TX FIFO Empty!!\r\n", __func__);
    }
}

void cb_i2s_inout_event(uint32_t event_info, uint32_t usr_param)
{
    uint8_t ch_dir;
    ch_dir = (event_info >> CSK_I2S_EVENT_XFER_DIR_POS) & 0x1;
    if (ch_dir == 1) { // IN
        cb_i2s_in_event(event_info, usr_param);
    } else { // OUT
        cb_i2s_out_event(event_info, usr_param);
    }
}

/**
 \fn          void cb_adc_dmic_in_event (uint32_t event, uint32_t usr_param)
 \brief       Signal ADC/PDM Events.
  \param[in]  event_info ADC/PDM event and channel information
              bit[7:0] is event type, bit[15:8] is ADC/PDM channel number
 \param[in]   usr_param    user parameter
 \return      none
*/
void cb_adc_dmic_in_event(uint32_t event_info, uint32_t usr_param)
{
    void *pdm_grp = (void*)usr_param;
    uint32_t rxd_len;
    uint8_t event, ch_no, *old_wp;
    bool buf_F = false;

    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;

    if(event & CSK_ADCPDM_EVENT_BLOCK_COMPLETE) {
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        memset(s_in_blks, 0, sizeof(s_in_blks));
        int32_t ret = ADC_PDM_PiPo_Xferred_Blocks(pdm_grp, s_in_blks, 2, ADC_BMP);
//        LOGD("RX BLK DONE, blk_cnt = %d, blk0: addr = 0x%x, samps = %d",
//                ret, s_in_blks[0].sample_data, s_in_blks[0].sample_cnt);

        assert(ret == 1);
        rxd_len = s_in_blks[0].sample_cnt * CHANNEL_BYTES;
        //LOGD("ADC BLK DONE, rx %d bytes", rxd_len);
        if (rxd_len > 0) {
            // update write pointer
            old_wp = s_wp;
            s_wp += rxd_len;
            if (s_wp >= s_limit)
                s_wp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_wp == s_rp) {
                s_buf_full = true; // audio buffer full
                buf_F = true;
            }

            // reuse last WR buffer to avoid recording cutoff
            if (s_buf_full)
                s_wp = old_wp;

            // trigger next dmic record operation
            //if (!s_stop_loop)
            trigger_adc_dmic_in_pipo(); // ADC PingPong RX

            // print log after trigger next operation
            if (buf_F) {
                LOGD("%s: audio buffer full, no space!!\r\n", __func__);
            }
        } // if (rxd_len > 0)
    }

    if(event & CSK_ADCPDM_EVENT_RECEIVE_COMPLETE){
        rxd_len = ADC_PDM_GetRxCount(pdm_grp, (0x1 << ch_no)) * CHANNEL_BYTES;
        //CLOGI("%s: [Done] receive %dB from DIN pin\r\n", __func__, rxd_len);
        if (rxd_len > 0) {
            // update write pointer
            old_wp = s_wp;
            s_wp += rxd_len;
            if (s_wp >= s_limit)
                s_wp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_wp == s_rp) {
                s_buf_full = true; // audio buffer full
                buf_F = true;
                //CLOGI("%s: audio buffer full, no audio data space!!\r\n", __func__);
                //LOGD("adc buf F\r\n");
            }

            // reuse last WR buffer to avoid recording cutoff
            if (s_buf_full)
                s_wp = old_wp;

            // trigger next dmic record operation
            if (!s_stop_loop)
                trigger_adc_dmic_in();

            // print log after trigger next operation
            if (buf_F) {
                LOGD("A buf F\n");
            }

/*
            //FOR TEST ONLY!!
            static uint32_t print_done = 0;
            if (print_done == 0)
            {
                print_done = 1;
                //HEXP(old_wp, 64);
                HEXP(old_wp, 16);
            }
*/

        } else { // (rxd_len == 0)
            LOGD("ADC RX no data!\r\n");
        }

    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) {
//        s_stop_loop = true;
//        ADC_PDM_Abort(pdm_grp, ADC_BMP);
        CLOGW("%s: ADC/DMIC RX FIFO Overflow, STOP!!\r\n", __func__);
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) {
        CLOGW("%s: ADC/DMIC RX FIFO Full!!\r\n", __func__);
    }
}


void cb_dac_out_event(uint32_t event_info, uint32_t usr_param)
{
    void *dac_grp = (void*)usr_param;
    uint32_t txd_len;
    uint16_t event;
    uint8_t ch_no, *old_rp;
    bool buf_E = false;

    event = event_info & CSK_DAC_EVENT_MASK;
    ch_no = (event_info >> CSK_DAC_EVENT_CH_POS) & 0x3;

    if(event & CSK_DAC_EVENT_BLOCK_COMPLETE){
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        memset(s_out_blks, 0, sizeof(s_out_blks));
        int32_t ret = DAC_PiPo_Xferred_Blocks(dac_grp, s_out_blks, 2, DAC_BMP);
//        LOGD("DAC BLK DONE");

        assert(ret == 1);
        txd_len = s_out_blks[0].sample_cnt * CHANNEL_BYTES;
        //LOGD("DAC BLK DONE, tx %d bytes", txd_len);
        if (txd_len > 0) {
            // update write pointer
            old_rp = s_rp;
            s_rp += txd_len;

            if (s_buf_full)
                s_buf_full = false;

            if (s_rp >= s_limit)
                s_rp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_rp == s_wp) {
                // reuse last RD buffer to avoid playback cutoff
                s_rp = old_rp;
                buf_E = true;
                //LOGD("buf E\r\n");
            }

            // trigger next i2s playback operation
            //if (!s_stop_loop)
            trigger_dac_out_pipo();

            // print log after trigger next operation
            if (buf_E) {
                LOGD("%s: audio buffer empty, no data!!\r\n", __func__);
            }
        } // if (txd_len > 0)
    }

    if(event & CSK_DAC_EVENT_SEND_COMPLETE){
        txd_len = DAC_GetTxCount(dac_grp, (0x1 << ch_no)) * CHANNEL_BYTES;
        //LOGD("%s: [Done] TXD %dB\r\n", __func__, txd_len);
        if (txd_len > 0) {
            // update write pointer
            old_rp = s_rp;
            s_rp += txd_len;

            if (s_buf_full)
                s_buf_full = false;

            if (s_rp >= s_limit)
                s_rp -= s_limit - (uint8_t*)s_aud_buf;

            if (s_rp == s_wp) {
                // reuse last RD buffer to avoid playback cutoff
                s_rp = old_rp;
                buf_E = true;
                //LOGD("%s: audio buffer empty, no audio data!!\r\n", __func__);
                //LOGD("DAC buf E\r\n");
            }

            // trigger next i2s playback operation
            if (!s_stop_loop)
                trigger_dac_out();

            // print log after trigger next operation
            if (buf_E) {
                LOGD("D buf E\n");
            }

        } // if (txd_len > 0)

    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
//        s_stop_loop = true;
//        DAC_Abort(dac_grp, DAC_BMP, 0);
        CLOGW("%s: DAC TX FIFO Underflow, STOP!!\r\n", __func__);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        CLOGW("%s: DAC TX FIFO Empty!!\r\n", __func__);
    }
}

//------------------------------------------------------------------
//  oneshot record/play functions & callbacks
//------------------------------------------------------------------
static volatile bool g_playdone = 0;
static volatile bool g_record_done = 0;

#define REC_SEC      1/2 //2 //1/2 //5 //
static uint32_t s_rec_buf [SAMPLE_RATE * 2 * CHANNEL_BYTES * REC_SEC / 4];
//static uint32_t s_rec_buf [SAMPLE_RATE * CHANNEL_COUNT * CHANNEL_BYTES * 20 / 4000]; // 100ms

#if (CHANNEL_BITS == 16 && CHANNEL_COUNT == 1)
// NiHaoMS16k16bit1ch.bin
#define AUDATA_OFFSET_1   0x20000 // 128KB
#define AUDATA_SIZE_1     (188362 & ~0x3UL)

#elif (CHANNEL_BITS == 32 && CHANNEL_COUNT == 1)
// NiHaoMS16k32bit1ch.bin, 32bit
#define AUDATA_OFFSET_1     0x40000     //256KB
#define AUDATA_SIZE_1       194228

#elif (CHANNEL_BITS == 16 && CHANNEL_COUNT == 2)
// NiHaoMS16k16bit2ch.bin, 32bit
#define AUDATA_OFFSET_1     0x80000     //512KB
#define AUDATA_SIZE_1       201452

#elif (CHANNEL_BITS == 32 && CHANNEL_COUNT == 2)
// NiHaoMS16k32bit2ch.bin, 32bit
#define AUDATA_OFFSET_1     0xC0000     //768KB
#define AUDATA_SIZE_1       398920

#else
#define AUDATA_OFFSET_1     0    //NOT USED
#define AUDATA_SIZE_1       0    //NOT USED

#endif

#define FLASH_BASE      0x30000000

bool i2s_oneshot_play()
{
    int32_t ret;

#if NOT_USE_PIPO_API
    const uint32_t *data;
    uint32_t num;

    // audio clip comes from static array of prepared audio data, 16Khz,16bit,stereo...

//    data = (const uint32_t *)g_audio_clip1_2;
//    num = sizeof(g_audio_clip1_2) / CHANNEL_BYTES;

//    data = (const uint32_t *)g_clip_earthquake_16k16b2ch;
//    num = sizeof(g_clip_earthquake_16k16b2ch) / CHANNEL_BYTES;

    data = (const uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1);
    num = AUDATA_SIZE_1 / CHANNEL_BYTES;

//    memset(s_rec_buf, 0xA5, sizeof(s_rec_buf));
//    data = (const uint32_t *)s_rec_buf;
//    num = sizeof(s_rec_buf) / CHANNEL_BYTES;

    //I2S_Control(g_i2s_out, CSK_I2S_TXCH_STEREO_SRC_STEREO, 0);
    ret = I2S_Send(g_i2s_out, data, num, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);

#else // !NOT_USE_PIPO_API

    memset(s_out_blks, 0, sizeof(s_out_blks));
    uint32_t *out_ptr = (uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1);
    uint32_t left_bytes = AUDATA_SIZE_1;
    uint32_t out_bytes;
    uint8_t i;

    s_out_bytes = 0;
    for (i=0; i<OUT_BLK_CNT; i++) {
        s_out_blks[i].sample_data = out_ptr;
        out_bytes = (left_bytes > OUT_STEP_BLK_BYTES ? OUT_STEP_BLK_BYTES : left_bytes);
        s_out_blks[i].sample_cnt = out_bytes / CHANNEL_BYTES;

        s_out_bytes += out_bytes;
        left_bytes -= out_bytes;
        out_ptr += (out_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = I2S_Send_PiPo(g_i2s_out, s_out_blks, &blk_cnt, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);
    assert(blk_cnt == OUT_BLK_CNT);

#endif // !NOT_USE_PIPO_API

/*
    data = (const uint32_t *)g_clip_earthquake_16k16b1ch;
    num = sizeof(g_clip_earthquake_16k16b1ch) / CHANNEL_BYTES;

//    I2S_Control(g_i2s_out, CSK_I2S_TXCH_MONO_SRC_MONO, 0);
//    ret = I2S_Send(g_i2s_out, data, num, CH_BMP_LEFT, I2S_TX_FLAG_START_NOW);
//    //ret = I2S_Send(g_i2s_out, data, num, CH_BMP_RIGHT, I2S_TX_FLAG_START_NOW);

    I2S_Control(g_i2s_out, CSK_I2S_TXCH_STEREO_SRC_MONO, 0);
    // only CH_BMP_STEREO is supported, and neither CH_BMP_LEFT nor CH_BMP_RIGHT is supported!
    ret = I2S_Send(g_i2s_out, data, num, CH_BMP_STEREO, I2S_TX_FLAG_START_NOW);
*/
    return (ret == CSK_DRIVER_OK);
}

void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event;
    uint8_t ch_no;
    int32_t ret;

    event = event_info & CSK_I2S_EVENT_MASK;
    ch_no = (event_info >> CSK_I2S_EVENT_I2S_CH_POS) & 0x3;

    if (event & CSK_I2S_EVENT_TX_BLOCK_COMPLETE) {
        uint32_t *out_ptr = (uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1 + s_out_bytes);
        uint32_t left_bytes = AUDATA_SIZE_1 - s_out_bytes;
        uint32_t out_bytes;
        uint8_t blk_cnt;

        if (left_bytes == 0) {
            LOGD("NO audio data filled in, wait I2S TX over...\n");

            I2S_Abort_Channels(i2s_dev, 0, I2S_OUT_BMP, 0);
            g_playdone = 1;

        } else {
            memset(s_out_blks, 0, sizeof(s_out_blks));
            s_out_blks[0].sample_data = out_ptr;
            //out_bytes = (left_bytes > OUT_BLK_BYTES ? OUT_BLK_BYTES : left_bytes);
            if (left_bytes > OUT_STEP_BLK_BYTES) {
                out_bytes = OUT_STEP_BLK_BYTES;
                //s_out_blks[0].flags = 0;
            } else {
                out_bytes = left_bytes;
                //s_out_blks[0].flags = 1;
            }
            s_out_blks[0].sample_cnt = out_bytes / CHANNEL_BYTES;

            s_out_bytes += out_bytes;
            //left_bytes -= out_bytes;

            blk_cnt = 1; //RENEW_BLK_CNT;
            ret = I2S_Send_PiPo(i2s_dev, s_out_blks, &blk_cnt, I2S_OUT_BMP, I2S_TX_FLAG_START_NOW);
            assert(ret == CSK_DRIVER_OK);
        } // left_bytes == 0
    } // event & CSK_I2S_EVENT_TX_BLOCK_COMPLETE

    if(event & CSK_I2S_EVENT_TRANSMIT_COMPLETE){
        g_playdone = 1;
        int32_t tx_cnt = I2S_GetTxCount(i2s_dev, (0x1 << ch_no));
        LOGD ("%s: I2S (ch_no = %d, tx = %d) Play DONE!!\n",
                __func__, ch_no, tx_cnt);
    }

    if (event & CSK_I2S_EVENT_TX_FIFO_UNDERRUN) {
        I2S_Abort_Channels(i2s_dev, 0, I2S_OUT_BMP, 0);
        CLOGW("%s: I2S TX FIFO Underflow!!\r\n", __func__);
    }

    if (event & CSK_I2S_EVENT_TX_FIFO_EMPTY) {
        CLOGW("%s: I2S TX FIFO Empty!!\r\n", __func__);
    }
}


bool i2s_oneshot_record()
{
    int32_t ret;
    uint32_t *data, num;

    data = (uint32_t *)s_rec_buf;
    num = sizeof(s_rec_buf) / CHANNEL_BYTES;
    memset(s_rec_buf, 0x88, sizeof(s_rec_buf));

#if NOT_USE_PIPO_API
    ret = I2S_Receive(g_i2s_in, (uint32_t*)data, num, I2S_IN_BMP, I2S_RX_FLAG_START_NOW); // 16bit: len/2
//    ret = I2S_Receive(g_i2s_in, (uint32_t*)data, num, CH_BMP_LEFT, I2S_RX_FLAG_START_NOW); // 16bit: len/2
//    ret = I2S_Receive(g_i2s_in, (uint32_t*)data, num, CH_BMP_RIGHT, I2S_RX_FLAG_START_NOW); // 16bit: len/2

    return (ret == CSK_DRIVER_OK);

#else // !NOT_USE_PIPO_API

    uint8_t i, blk_cnt;
    uint32_t in_bytes = IN_STEP_BLK_BYTES;

    s_in_bytes = 0;
    memset(s_in_blks, 0, sizeof(s_in_blks));
    for (i=0; i<IN_BLK_CNT; i++) {
        s_in_blks[i].sample_data = data;
        s_in_blks[i].sample_cnt = in_bytes / CHANNEL_BYTES;
        s_in_bytes += in_bytes;
        data += (in_bytes + 3) / 4;
    }

    blk_cnt = i;
    ret = I2S_Receive_PiPo(g_i2s_in, s_in_blks, &blk_cnt, I2S_IN_BMP, I2S_RX_FLAG_START_NOW);
    assert(blk_cnt == IN_BLK_CNT);
    return (ret == CSK_DRIVER_OK);

#endif // !NOT_USE_PIPO_API
}

void cb_i2s_oneshot_record_event(uint32_t event_info, uint32_t usr_param)
{
    void *i2s_dev = (void*)usr_param;
    uint16_t event;
    uint8_t ch_no;
    int32_t rx_cnt;
    int32_t ret;

    event = event_info & CSK_I2S_EVENT_MASK;
    ch_no = (event_info >> CSK_I2S_EVENT_I2S_CH_POS) & 0x3;

    if(event & CSK_I2S_EVENT_RX_BLOCK_COMPLETE) {
        uint32_t left_bytes = sizeof(s_rec_buf) - s_in_bytes;
        uint32_t *data;
        uint32_t in_bytes;
        uint8_t blk_cnt;

        if (left_bytes == 0) {
#if 0
            //FOR TEST & RETRIEVE AUDIO DATA ONLY!!
            static int count = 0;
            if (count++ == 50) {
                I2S_Abort_Channels(i2s_dev, I2S_IN_BMP, 0, 0); // abort current record
                LOGD("STOP for export audio\n");
                return;
            }
#endif
            LOGD("I2S RX BLK DONE (NO space!)");
            I2S_Abort_Channels(i2s_dev, I2S_IN_BMP, 0, 0);
            g_record_done = 1;

        } else {
            LOGD("I2S RX BLK DONE");
            data = (uint32_t *)&s_rec_buf[s_in_bytes/4];

            memset(s_in_blks, 0, sizeof(s_in_blks));
            s_in_blks[0].sample_data = data;
            if (left_bytes > IN_STEP_BLK_BYTES) {
                in_bytes = IN_STEP_BLK_BYTES;
                //s_in_blks[0].flags = 0;
            } else {
                in_bytes = left_bytes;
                //s_in_blks[0].flags = 1;
            }
            s_in_blks[0].sample_cnt = in_bytes / CHANNEL_BYTES;

            s_in_bytes += in_bytes;
            blk_cnt = 1; //RENEW_BLK_CNT;
            ret = I2S_Receive_PiPo(i2s_dev, s_in_blks, &blk_cnt, I2S_IN_BMP, I2S_RX_FLAG_START_NOW);
            assert(ret == CSK_DRIVER_OK);
        }
    }

    if(event & CSK_I2S_EVENT_RECEIVE_COMPLETE){
        g_record_done = 1;
        rx_cnt = I2S_GetRxCount(i2s_dev, (0x1 << ch_no));
        LOGD ("%s: I2S (ch_no = %d, rx = %d) Record DONE!!\n",
                __func__, ch_no, rx_cnt);
    }

    if (event & CSK_I2S_EVENT_RX_FIFO_OVERRUN) {
        I2S_Abort_Channels(i2s_dev, I2S_IN_BMP, 0, 0);
        CLOGW("%s: I2S RX FIFO Overflow!!\r\n", __func__);
    }

    if (event & CSK_I2S_EVENT_RX_FIFO_FULL) {
        CLOGW("%s: I2S RX FIFO Full!!\r\n", __func__);
    }
}

bool dmic_oneshot_record()
{
    int32_t ret;
    uint32_t *data, num;

    data = (uint32_t *)s_rec_buf;
    num = sizeof(s_rec_buf) / CHANNEL_BYTES;

#if NOT_USE_PIPO_API
    ret = ADC_PDM_Receive(g_adc_dmic_in, data, num, PDM_BMP, ADC_PDM_RX_FLAG_START_NOW);
    return (ret == CSK_DRIVER_OK);

#else // !NOT_USE_PIPO_API

    uint8_t i, blk_cnt;
    uint32_t in_bytes = IN_STEP_BLK_BYTES;

    s_in_bytes = 0;
    memset(s_in_blks, 0, sizeof(s_in_blks));
    for (i=0; i<IN_BLK_CNT; i++) {
        s_in_blks[i].sample_data = data;
        s_in_blks[i].sample_cnt = in_bytes / CHANNEL_BYTES;
        s_in_bytes += in_bytes;
        data += (in_bytes + 3) / 4;
    }

    blk_cnt = i;
    ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt,
                                PDM_BMP, ADC_PDM_RX_FLAG_START_NOW);
    assert(blk_cnt == IN_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
#endif // !NOT_USE_PIPO_API
}

void cb_dmic_oneshot_record_event(uint32_t event_info, uint32_t usr_param)
{
    void *pdm_grp = (void*)usr_param;
    uint8_t event, ch_no;
    int32_t ret;

    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;

    if(event & CSK_ADCPDM_EVENT_BLOCK_COMPLETE) {
        uint32_t *data = (uint32_t *)&s_rec_buf[s_in_bytes/4];
        uint32_t left_bytes = sizeof(s_rec_buf) - s_in_bytes;
        uint32_t in_bytes;
        uint8_t blk_cnt;

        if (left_bytes == 0) {
            LOGD("DMIC BLK DONE (NO space!)");

            ADC_PDM_Abort(pdm_grp, PDM_BMP);
            g_record_done = 1;

        } else {
            LOGD("DMIC BLK DONE");
            memset(s_in_blks, 0, sizeof(s_in_blks));
            s_in_blks[0].sample_data = data;
            if (left_bytes > IN_STEP_BLK_BYTES) {
                in_bytes = IN_STEP_BLK_BYTES;
                //s_in_blks[0].flags = 0;
            } else {
                in_bytes = left_bytes;
                //s_in_blks[0].flags = 1;
            }
            s_in_blks[0].sample_cnt = in_bytes / CHANNEL_BYTES;
            s_in_bytes += in_bytes;

            blk_cnt = 1; //IN_RENEW_BLK_CNT;
            ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt,
                                        PDM_BMP, ADC_PDM_RX_FLAG_START_NOW);
            assert(ret == CSK_DRIVER_OK);
        }
    }

    if(event & CSK_ADCPDM_EVENT_RECEIVE_COMPLETE){
        g_record_done = 1;
        int32_t rx_cnt = ADC_PDM_GetRxCount(pdm_grp, PDM_BMP);
        LOGD ("%s: PDM/DMIC (ch_no = %d, rx = %d) Record DONE!!\n",
                __func__, ch_no, rx_cnt);
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) {
        ADC_PDM_Abort(pdm_grp, PDM_BMP);
        CLOGW("%s: PDM/DMIC RX FIFO Overflow!!\r\n", __func__);
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) {
        CLOGW("%s: PDM/DMIC RX FIFO Full!!\r\n", __func__);
    }
}

bool adc_oneshot_record()
{
    int32_t ret;
    uint32_t *data, num;

    data = (uint32_t *)s_rec_buf;
    num = sizeof(s_rec_buf) / CHANNEL_BYTES;
    memset(s_rec_buf, 0x88, sizeof(s_rec_buf));

#if NOT_USE_PIPO_API
    ret = ADC_PDM_Receive(g_adc_dmic_in, data, num, ADC_BMP, ADC_PDM_RX_FLAG_START_NOW);
    return (ret == CSK_DRIVER_OK);

#else // !NOT_USE_PIPO_API

    uint8_t i, blk_cnt;
    uint32_t in_bytes = IN_STEP_BLK_BYTES;

    s_in_bytes = 0;
    memset(s_in_blks, 0, sizeof(s_in_blks));
    for (i=0; i<IN_BLK_CNT; i++) {
        s_in_blks[i].sample_data = data;
        s_in_blks[i].sample_cnt = in_bytes / CHANNEL_BYTES;
        s_in_bytes += in_bytes;
        data += (in_bytes + 3) / 4;
    }

    blk_cnt = i;
    ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt,
                                ADC_BMP, ADC_PDM_RX_FLAG_START_NOW);
    assert(blk_cnt == IN_BLK_CNT);
    return (ret == CSK_DRIVER_OK);
#endif // !NOT_USE_PIPO_API

}

void cb_adc_oneshot_record_event(uint32_t event_info, uint32_t usr_param)
{
    void *adc_grp = (void*)usr_param;
    uint8_t event, ch_no;
    int32_t ret;

    event = event_info & 0xFF;
    ch_no = (event_info >> 8) & 0xFF;

    if(event & CSK_ADCPDM_EVENT_BLOCK_COMPLETE) {
        uint32_t *data = (uint32_t *)&s_rec_buf[s_in_bytes/4];
        uint32_t left_bytes = sizeof(s_rec_buf) - s_in_bytes;
        uint32_t in_bytes;
        uint8_t blk_cnt;

/*
        memset(s_in_blks, 0, sizeof(s_in_blks));
        //NOTE: PiPo_Xferred_Blocks return the count of transferred block since last BLOCK COMPLETE event!
        ret = ADC_PDM_PiPo_Xferred_Blocks(adc_grp, s_in_blks, 2);
        assert(ret > 0);
*/

        if (left_bytes == 0) {
/*
            LOGD("ADC BLK DONE (NO buffer!), blk_cnt = %d, blk0: addr = 0x%x, samps = %d",
                    ret, s_in_blks[0].sample_data, s_in_blks[0].sample_cnt);
*/
            LOGD("ADC BLK DONE (NO space!)");

            ADC_PDM_Abort(adc_grp, ADC_BMP);
            g_record_done = 1;

        } else {
/*
            LOGD("ADC BLK DONE, blk_cnt = %d, blk0: addr = 0x%x, samps = %d",
                    ret, s_in_blks[0].sample_data, s_in_blks[0].sample_cnt);
*/
//            LOGD("ADC BLK DONE");

            memset(s_in_blks, 0, sizeof(s_in_blks));
            s_in_blks[0].sample_data = data;
            if (left_bytes > IN_STEP_BLK_BYTES) {
                in_bytes = IN_STEP_BLK_BYTES;
                //s_in_blks[0].flags = 0;
            } else {
                in_bytes = left_bytes;
                //s_in_blks[0].flags = 1;
            }
            s_in_blks[0].sample_cnt = in_bytes / CHANNEL_BYTES;

            s_in_bytes += in_bytes;
            //left_bytes -= in_bytes;

            blk_cnt = 1; //IN_RENEW_BLK_CNT;
            ret = ADC_PDM_Receive_PiPo(g_adc_dmic_in, s_in_blks, &blk_cnt,
                                        ADC_BMP, ADC_PDM_RX_FLAG_START_NOW);
            assert(ret == CSK_DRIVER_OK);
        }
    }

    if(event & CSK_ADCPDM_EVENT_RECEIVE_COMPLETE){
        g_record_done = 1;
        int32_t rx_cnt = ADC_PDM_GetRxCount(adc_grp, ADC_BMP);
        LOGD ("%s: ADC (ch_no = %d, rx = %d) Record DONE!!\n",
                __func__, ch_no, rx_cnt);
/*
        //FOR TEST ONLY!!
        static uint32_t print_done = 0;
        if (print_done == 0)
        {
            print_done = 1;
            //HEXP(s_rec_buf, 64);
            HEXP(s_rec_buf, 16);
        }
*/
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) {
        ADC_PDM_Abort(adc_grp, ADC_BMP);
        CLOGW("%s: ADC RX FIFO Overflow!!\r\n", __func__);
    }

    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) {
        CLOGW("%s: ADC RX FIFO Full!!\r\n", __func__);
    }
}

bool dac_oneshot_play()
{
    int32_t ret;

#if NOT_USE_PIPO_API
    const uint32_t *data;
    uint32_t num;

    // audio clip comes from static array of prepared audio data, 16Khz,16bit,stereo...

    data = (const uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1);
    num = AUDATA_SIZE_1 / CHANNEL_BYTES;

    ret = DAC_Send(g_dac_out, data, num, DAC_BMP, DAC_TX_FLAG_START_NOW);

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

#else // !NOT_USE_PIPO_API

    memset(s_out_blks, 0, sizeof(s_out_blks));
    uint32_t *out_ptr = (uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1);
    uint32_t left_bytes = AUDATA_SIZE_1;
    uint32_t out_bytes;
    uint8_t i;

    s_out_bytes = 0;
    for (i=0; i<OUT_BLK_CNT; i++) {
        s_out_blks[i].sample_data = out_ptr;
        out_bytes = (left_bytes > OUT_STEP_BLK_BYTES ? OUT_STEP_BLK_BYTES : left_bytes);
        s_out_blks[i].sample_cnt = out_bytes / CHANNEL_BYTES;

        s_out_bytes += out_bytes;
        left_bytes -= out_bytes;
        out_ptr += (out_bytes + 3) / 4;
    }

    uint8_t blk_cnt = i;
    ret = DAC_Send_PiPo(g_dac_out, s_out_blks, &blk_cnt, DAC_BMP, DAC_TX_FLAG_START_NOW);
    assert(blk_cnt == OUT_BLK_CNT);
    return (ret == CSK_DRIVER_OK);

#endif // !NOT_USE_PIPO_API
}

void cb_dac_oneshot_play_event(uint32_t event_info, uint32_t usr_param)
{
    void *dac_grp = (void*)usr_param;
    uint16_t event;
    uint8_t ch_no;
    int32_t ret;

    event = event_info & CSK_DAC_EVENT_MASK;
    ch_no = (event_info >> CSK_DAC_EVENT_CH_POS) & 0x3;

    if (event & CSK_DAC_EVENT_BLOCK_COMPLETE) {
        uint32_t *out_ptr = (uint32_t *)(FLASH_BASE + AUDATA_OFFSET_1 + s_out_bytes);
        uint32_t left_bytes = AUDATA_SIZE_1 - s_out_bytes;
        uint32_t out_bytes;
        uint8_t blk_cnt;

        if (left_bytes == 0) {
            LOGD("NO audio data filled in, wait DAC TX over...\n");

//            DAC_Abort(dac_grp, DAC_BMP, 0);
//            g_playdone = 1;

        } else {
            memset(s_out_blks, 0, sizeof(s_out_blks));
            s_out_blks[0].sample_data = out_ptr;
            //out_bytes = (left_bytes > OUT_BLK_BYTES ? OUT_BLK_BYTES : left_bytes);
            if (left_bytes > OUT_STEP_BLK_BYTES) {
                out_bytes = OUT_STEP_BLK_BYTES;
                s_out_blks[0].flags = 0;
            } else {
                out_bytes = left_bytes;
                s_out_blks[0].flags = 1;
            }
            s_out_blks[0].sample_cnt = out_bytes / CHANNEL_BYTES;

            s_out_bytes += out_bytes;
            //left_bytes -= out_bytes;

            blk_cnt = 1; //RENEW_BLK_CNT;
            ret = DAC_Send_PiPo(dac_grp, s_out_blks, &blk_cnt, DAC_BMP, DAC_TX_FLAG_START_NOW);
            assert(ret == CSK_DRIVER_OK);
        } // left_bytes == 0
    } // event & CSK_DAC_EVENT_TX_BLOCK_COMPLETE

    if(event & CSK_DAC_EVENT_SEND_COMPLETE){
        g_playdone = 1;
        int32_t rx_cnt = DAC_GetTxCount(dac_grp, DAC_BMP);
        LOGD ("%s: DAC (ch_no = %d, rx = %d) Play DONE!!\n",
                __func__, ch_no, rx_cnt);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        DAC_Abort(dac_grp, DAC_BMP, 0);
        CLOGW("%s: DAC TX FIFO Underflow!!\r\n", __func__);
    }

    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) {
        CLOGW("%s: DAC TX FIFO Empty!!\r\n", __func__);
    }
}

//------------------------------------------------------------------
int main()
{
    bool ret, ext_codec = EXT_CODEC; // is there any external CODEC with I2S interface?

    //iomux_reg = (CORE_IOMUX_RegDef*)CMN_IOMUX_BASE;

/*
    // Init SYSPLL core clock
    SYSPLL_InitCore(CRM_IpCore_120MHz);
    // configure the CORE/HCLK clock source with SYSPLL
    HAL_CRM_SetHclkClkSrc(CRM_IpSrcSysPllCore);
*/

/*
    // disable the FLASH clock
    __HAL_CRM_FLASH_CLK_DISABLE();
    // init QSPI/FLASH clock
    SYSPLL_InitFlash(CRM_IpFlash_100MHz); //100MHz
    // enable the FLASH clock
    __HAL_CRM_FLASH_CLK_ENABLE();
*/


    //uint32_t uart_idx = 0;
    uint32_t uart_idx = 0;
    logInit(uart_idx, 115200);
    LOGD("use UART%d as log print!\n", uart_idx);

    // enable global interrupts (all levels of interrupts)
    enable_GINT();

    nos_timer_init();

    if (ext_codec) {
        // Get I2C device to setup CODEC
        g_i2c_dev = get_i2c_dev();
        if (g_i2c_dev == NULL)
            return -1;

        // Initialize I2C device
        ret = init_i2c_dev(g_i2c_dev);
        if (!ret)
            return -1;
    }

    gpio_A = GPIOA();
    GPIO_Initialize(gpio_A, NULL, NULL);

    //gpio_B = GPIOB();
    //GPIO_Initialize(gpio_B, NULL, NULL);

#if (CUR_AIOT == AIOT_ADC_IN_I2S_OUT) // adc in & i2s out
    LOGD("larryba test ADC->I2S, %dk/%dch/%dbit ---\r\n", SAMPLE_RATE/1000, (ADC_BMP==ADC_PDM_BMP_STEREO ? 2 : 1), CHANNEL_BITS);

    // Initialize ADC IN device group, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
    g_adc_dmic_in = init_adc_in(ADC_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, cb_adc_dmic_in_event);
    if (g_adc_dmic_in == NULL)
        return -1;

    // Initialize I2S OUT device (including WM8960 CODEC, spk_out_bmp != 0) or
    // Initialize I2S OUT device only (no external CODEC, spk_out_bmp = 0)
    g_i2s_out = init_i2s_wm8960_out(true, I2S_OUT_BMP, (ext_codec ? I2S_OUT_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_out_event);
//    g_i2s_out = init_i2s_wm8960_out(true, I2S_OUT_BMP, (ext_codec ? I2S_OUT_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_out_event);
    if (g_i2s_out == NULL)
        return -1;

    // Initialize audio buffer
    init_audio_buffer();

    nos_delay_ms(100); //ms

    s_stop_loop = false;
    LOGD("TEST ADC_IN_I2S_OUT...\n");

    // Trigger ADC recording
#if NOT_USE_PIPO_API
    trigger_adc_dmic_in();
#else
    init_adc_dmic_in_pipo();
#endif

    // Trigger I2S playback
#if NOT_USE_PIPO_API
    trigger_i2s_out();
#else
    init_i2s_out_pipo();
#endif

#elif (CUR_AIOT == AIOT_DMIC_IN_I2S_OUT) // dmic in & i2s out
    LOGD("larryba test DMIC->I2S, %dk/%dch/%dbit ---\r\n", SAMPLE_RATE/1000, (ADC_BMP==ADC_PDM_BMP_STEREO ? 2 : 1), CHANNEL_BITS);

    // Initialize DMIC IN device, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
    g_adc_dmic_in = init_dmic_in(PDM_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                            SAMPLE_RATE, cb_adc_dmic_in_event);
    if (g_adc_dmic_in == NULL)
        return -1;

    // Initialize I2S OUT device (including WM8960 CODEC) or I2S OUT device only (no external CODEC)
    g_i2s_out = init_i2s_wm8960_out(true, I2S_OUT_BMP, (ext_codec ? I2S_OUT_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_out_event);
    if (g_i2s_out == NULL)
        return -1;

    // Initialize audio buffer
    init_audio_buffer();

    s_stop_loop = false;

    // Trigger DMIC recording
#if NOT_USE_PIPO_API
    trigger_adc_dmic_in();
#else
    init_adc_dmic_in_pipo();
#endif

    // Trigger I2S playback
#if NOT_USE_PIPO_API
    trigger_i2s_out();
#else
    init_i2s_out_pipo();
#endif
    LOGD("TEST DMIC_IN_I2S_OUT...\n");

#elif (CUR_AIOT == AIOT_I2S_IN_DAC_OUT) // i2s in & dac out
    LOGD("larryba test I2S->DAC, %dk/%dch/%dbit ---\r\n", SAMPLE_RATE/1000, (DAC_BMP==DAC_BMP_STEREO ? 2 : 1), CHANNEL_BITS);

    // Initialize I2S IN device (including ES7210 CODEC) or I2S IN device (no external CODEC if mic_in_bmp == 0)
//    g_i2s_in = init_i2s_es7210_in(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_in_event);
//    g_i2s_in = init_i2s_wm8960_in(true, I2S_IN_BMP, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_in_event); // I2S master
    g_i2s_in = init_i2s_wm8960_in(true, I2S_IN_BMP, (ext_codec ? I2S_IN_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_in_event); //I2S slave
    if (g_i2s_in == NULL)
        return -1;

    // Initialize DAC OUT device group, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
//    g_dac_out = init_dac_out(DAC_BMP_STEREO, (CHANNEL_BITS == 16 ? 1 : 0), SAMPLE_RATE, cb_dac_out_event);
    g_dac_out = init_dac_out(DAC_BMP, (CHANNEL_BITS == 16 ? 1 : 0), SAMPLE_RATE, cb_dac_out_event);
    if (g_dac_out == NULL)
        return -1;

    nos_delay_ms(100); //ms

    s_stop_loop = false;
    LOGD("TEST I2S_IN_DAC_OUT...\n");

    // Initialize audio buffer
    init_audio_buffer();

    // Trigger I2S recording
#if NOT_USE_PIPO_API
    trigger_i2s_in();
#else
    init_i2s_in_pipo();
#endif

    // Trigger DAC playback
#if NOT_USE_PIPO_API
    trigger_dac_out();
#else
    init_dac_out_pipo();
#endif

#elif (CUR_AIOT == AIOT_ADC_IN_DAC_OUT) // adc in & dac out
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);

    // Initialize ADC IN device group, support 24 bits sample (act as 32 bits) only
    g_adc_dmic_in = init_adc_in(ADC_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                                SAMPLE_RATE, cb_adc_dmic_in_event);
    if (g_adc_dmic_in == NULL)
        return -1;

    // Initialize DAC OUT device group, support 24 bits sample (act as 32 bits) only
    g_dac_out = init_dac_out(DAC_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                                SAMPLE_RATE, cb_dac_out_event);
    if (g_dac_out == NULL)
        return -1;

    // Initialize audio buffer
    init_audio_buffer();

    s_stop_loop = false;

    // Trigger ADC recording
#if NOT_USE_PIPO_API
    trigger_adc_dmic_in();
#else
    init_adc_dmic_in_pipo();
#endif

    // Trigger DAC playback
#if NOT_USE_PIPO_API
    trigger_dac_out();
#else
    init_dac_out_pipo();
#endif

    LOGD("TEST ADC_IN_DAC_OUT...\n");

#elif (CUR_AIOT == AIOT_I2S_IN_I2S_OUT) // i2s in & i2s out

    // Initialize I2S IN (including ES7210 CODEC) device
    g_i2s_in = init_i2s_es7210_in(true, I2S_IN_BMP, (ext_codec ? I2S_IN_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_in_event);
    if (g_i2s_in == NULL)
        return -1;

    // Initialize I2S OUT (including WM8960 CODEC) device
    g_i2s_out = init_i2s_wm8960_out(true, I2S_OUT_BMP, (ext_codec ? I2S_OUT_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_out_event);
    if (g_i2s_out == NULL)
        return -1;

    // Initialize audio buffer
    init_audio_buffer();

    s_stop_loop = false;

    // Trigger I2S recording
#if NOT_USE_PIPO_API
    trigger_i2s_in();
#else
    init_i2s_in_pipo();
#endif

    // Trigger I2S playback
#if NOT_USE_PIPO_API
    trigger_i2s_out();
#else
    init_i2s_out_pipo();
#endif

#elif (CUR_AIOT == AIOT_ONESHOT_ADC_IN) // oneshot ADC record

    // Initialize ADC IN device group, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
    g_adc_dmic_in = init_adc_in(ADC_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, cb_adc_oneshot_record_event);
    if (g_adc_dmic_in == NULL)
        return -1;

    while (1) {
      LOGD("ADC RECORD...\n");
      g_record_done = 0;
      if (!adc_oneshot_record())
           return -1;
       nos_delay_ms(3000); // delay 3 sec.
       while (!g_record_done);
    }

//    LOGD("ADC group Recording...\n");
//    adc_oneshot_record();
//    while (!g_record_done);

    nos_delay_ms(3000); // delay 3 sec.
    ADC_PDM_Uninitialize(g_adc_dmic_in);

#elif (CUR_AIOT == AIOT_ONESHOT_DMIC_IN) // oneshot DMIC record

    // Initialize DMIC IN device group, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
    g_adc_dmic_in = init_dmic_in(PDM_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, cb_dmic_oneshot_record_event);
    if (g_adc_dmic_in == NULL)
        return -1;

    LOGD("DMIC Recording...\n");
    dmic_oneshot_record();
    while (!g_record_done);

    nos_delay_ms(3000); // delay 3 sec.
    ADC_PDM_Uninitialize(g_adc_dmic_in);

#elif (CUR_AIOT == AIOT_ONESHOT_I2S_IN) // oneshot I2S record

    // Initialize I2S IN (including WM8960 CODEC) device
//    g_i2s_in = init_i2s_wm8960_in(false, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, I2S Justified
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_record_event); //master, I2S Justified
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_LEFT, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_record_event);
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_RIGHT, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_record_event);

//    g_i2s_in = init_i2s_wm8960_in(false, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, LJ
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_oneshot_record_event); //master, LJ

//    g_i2s_in = init_i2s_wm8960_in(false, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_RIGHT, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, RJ
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_RIGHT, SAMPLE_RATE, cb_i2s_oneshot_record_event); //master, RJ

//    g_i2s_in = init_i2s_wm8960_in(false, CH_BMP_STEREO, CH_BMP_STEREO, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_A, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, TDM, A
//    g_i2s_in = init_i2s_wm8960_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_A, SAMPLE_RATE, cb_i2s_oneshot_record_event); // master, TDM, A

//    g_i2s_in = init_i2s_wm8960_in(false, CH_BMP_STEREO, CH_BMP_STEREO, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_B, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, TDM, B
    g_i2s_in = init_i2s_wm8960_in(true, I2S_IN_BMP, (ext_codec ? I2S_IN_BMP : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_B, SAMPLE_RATE, cb_i2s_oneshot_record_event); // master, TDM, B
//    g_i2s_in = init_i2s_wm8960_in(true, CHANNEL_BMP, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_B, SAMPLE_RATE, cb_i2s_oneshot_record_event); //slave, TDM, B


//    g_i2s_in = init_i2s_es7210_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_record_event);
//    g_i2s_in = init_i2s_es7210_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_oneshot_record_event);
//    g_i2s_in = init_i2s_es7210_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_A, SAMPLE_RATE, cb_i2s_oneshot_record_event); // master, TDM, A
//    g_i2s_in = init_i2s_es7210_in(true, CH_BMP_STEREO, CH_BMP_STEREO, 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_B, SAMPLE_RATE, cb_i2s_oneshot_record_event); // master, TDM, B

    if (g_i2s_in == NULL)
        return -1;

    LOGD("I2S Recording...\n");
    i2s_oneshot_record();
    while (!g_record_done);

    nos_delay_ms(5000); // delay 5 sec.
    I2S_Uninitialize(g_i2s_in);

#elif (CUR_AIOT == AIOT_ONESHOT_DAC_OUT) // oneshot DAC play

    // Initialize DAC OUT device group, support 24 bits sample (act as 32 bits) only
    assert(CHANNEL_BITS == 16 || CHANNEL_BITS == 32);
    g_dac_out = init_dac_out(DAC_BMP, (CHANNEL_BITS == 16 ? 1 : 0),
                        SAMPLE_RATE, cb_dac_oneshot_play_event);
    if (g_dac_out == NULL)
        return -1;

    while (1) {
      LOGD("DAC PLAY\n");
      g_playdone = 0;
      if (!dac_oneshot_play())
           return -1;
       nos_delay_ms(3000); // delay 3 sec.
       while (!g_playdone);
    }

//    LOGD("DAC PLAY\n");
//    dac_oneshot_play();
//    while (!g_playdone);

    nos_delay_ms(5000); // delay 5 sec.
    DAC_Uninitialize(g_dac_out);

#elif (CUR_AIOT == AIOT_ONESHOT_I2S_OUT) // oneshot I2S play

    // Initialize I2S OUT (including WM8960 CODEC) device
//    g_i2s_out = init_i2s_wm8960_out(false, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_play_event); //slave, I2S J
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_play_event); //master, I2S J
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_LEFT : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_play_event);
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_RIGHT : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_play_event);

//    g_i2s_out = init_i2s_wm8960_out(false, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_oneshot_play_event); //slave, LJ
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_LEFT, SAMPLE_RATE, cb_i2s_oneshot_play_event); //master, LJ

//    g_i2s_out = init_i2s_wm8960_out(false, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_RIGHT, SAMPLE_RATE, cb_i2s_oneshot_play_event); //slave, RJ // NO output?
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 0, CHANNEL_BITS, I2S_PROTO_RIGHT, SAMPLE_RATE, cb_i2s_oneshot_play_event); //master, RJ

//    g_i2s_out = init_i2s_wm8960_out(false, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_0, SAMPLE_RATE, cb_i2s_oneshot_play_event); //slave, TDM, A
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_0, SAMPLE_RATE, cb_i2s_oneshot_play_event); // master, TDM, A
//    g_i2s_out = init_i2s_wm8960_out(true, 0xF, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_0, SAMPLE_RATE, cb_i2s_oneshot_play_event); // master, TDM4, A
//    g_i2s_out = init_i2s_wm8960_out(true, 0xF, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_0, SAMPLE_RATE, cb_i2s_oneshot_play_event); // master, TDM4, A

//    g_i2s_out = init_i2s_wm8960_out(false, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_1, SAMPLE_RATE, cb_i2s_oneshot_play_event); //slave, TDM, B
//    g_i2s_out = init_i2s_wm8960_out(true, CH_BMP_STEREO, (ext_codec ? CH_BMP_STEREO : 0), 1, CHANNEL_BITS, I2S_PROTO_PCMMODE_1, SAMPLE_RATE, cb_i2s_oneshot_play_event); // master, TDM, B

    g_i2s_out = init_i2s_wm8960_out(true, I2S_OUT_BMP, (ext_codec ? I2S_OUT_BMP : 0), 0, CHANNEL_BITS, I2S_PROTO_PHILIPS, SAMPLE_RATE, cb_i2s_oneshot_play_event); //master, I2S J

    if (g_i2s_out == NULL)
        return -1;

    while (1) {
      LOGD("I2S PLAY\n");
      g_playdone = 0;
      if (!i2s_oneshot_play())
           return -1;
       while (!g_playdone);
       nos_delay_ms(5000); // delay 5 sec.
    }

//    LOGD("I2S PLAY\n");
//    i2s_oneshot_play();
//    while (!g_playdone);
//    nos_delay_ms(5000); // delay 5 sec.

    I2S_Uninitialize(g_i2s_out);
#endif

    while(1); // DON'T EXIT, or else it may enter exception handler!

    if (gpio_A != NULL) {
        GPIO_Uninitialize(gpio_A);
        gpio_A = NULL;
    }

    //if (gpio_B != NULL) {
    //    GPIO_Uninitialize(gpio_B);
    //    gpio_B = NULL;
    //}

    return 0;
}
