#include <string.h>
#include <assert.h>
#include "log_print.h"
#include "systick.h"
#include "Driver_ADC_PDM.h"
#include "Driver_GPIO.h"
#include "Driver_Common.h"
#include "IOMuxManager.h"
#include "cache.h"

#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "stream_buffer.h"

#include "esp_heap_caps_init.h"
#include "lite_adc.h"
#include "arcs_ap.h"

#define TAG "ADC"
#include "lisa_log.h"
// #define CONFIG_AADC_GAIN_A 30
// #define CONFIG_AADC_GAIN_D 0
#define GPDMA_ADC0_CHN    (CONFIG_GPDMA_ADC0_CHN)
#define GPDMA_ADC1_CHN    (CONFIG_GPDMA_ADC1_CHN)
#define DMA_CHANNEL_ANY   (0xFF)

// #define CONFIG_AADC_HPF    1
// #define CONFIG_AADC_DIFF   1
// #define CONFIG_AADC_PIPO   1

// #define CONFIG_AADC_L_ONLY 1
// #define CONFIG_AADC_CHNS   1
#ifdef CONFIG_LITE_AADC_RECV_CNTS
#define AADC_RECV_CNTS      (CONFIG_LITE_AADC_RECV_CNTS)
#else
#define AADC_RECV_CNTS      (12)
#endif
#define AADC_STEP_SAMP      (CONFIG_AUDIO_STEP_SAMPS * CONFIG_AADC_CHNS)
#define AADC_STEP_SIZE      (sizeof(aud_samp_t) * AADC_STEP_SAMP)
#if CONFIG_AADC_L_ONLY
    #define AADC_DEV_BMP    CH_BMP_LEFT
#elif CONFIG_AADC_R_ONLY
    #define AADC_DEV_BMP    CH_BMP_RIGHT
#elif CONFIG_AADC_LR_MIX
    #define AADC_DEV_BMP    CH_BMP_STEREO
#else
    #error invalid
#endif


#define REC_ASSERT(cond, code)                \
	do {                                                \
		if (!(cond)) {                                  \
			CLOGE("[%s %d]"#cond, __FILE__, __LINE__); \
			code;                                       \
		}                                               \
	} while (0)

#define ASSERT(exp, fmt, ...)	do{if(!(exp)){CLOGE("error:"fmt,##__VA_ARGS__);assert(exp);}}while(0)

static struct
{
    void* hdrv;
    void* fifo[AADC_RECV_CNTS];
    int xpos;
    bool xfer;
    QueueHandle_t xque;
}lite_adc;

static void aadc_drv_event(uint32_t event, uint32_t user)
{
    BaseType_t yield = pdFALSE;
    int ret = CSK_DRIVER_OK;

    if (event & (CSK_ADCPDM_EVENT_RECEIVE_COMPLETE
    #if CONFIG_AADC_PIPO
        | CSK_ADCPDM_EVENT_BLOCK_COMPLETE
    #endif
    )) {
        void* recv = lite_adc.fifo[lite_adc.xpos];
        if (++lite_adc.xpos >= AADC_RECV_CNTS) lite_adc.xpos = 0;
        int ipos = lite_adc.xpos;
        if (++ipos >= AADC_RECV_CNTS) ipos = 0;

    #if CONFIG_AADC_PIPO
        ret = ADC_PDM_Receive_PiPo(lite_adc.hdrv
            , &(PIPO_IN_BLOCK){ .sample_data = lite_adc.fifo[ipos], .sample_cnt = AADC_STEP_SAMP, .flags = 0 }
            , &(uint8_t){1}, AADC_DEV_BMP, ADC_PDM_RX_FLAG_START_NOW);
    #else
        ret = ADC_PDM_Receive(lite_adc.hdrv, lite_adc.fifo[ipos], AADC_STEP_SAMP, AADC_DEV_BMP, ADC_PDM_RX_FLAG_START_NOW);
    #endif
        if(ret != CSK_DRIVER_OK){
            CLOG("aadc_drv_event ret:%d", ret);
        }

        dcache_invalidate_range((uint32_t)recv, ((uint32_t)recv) + AADC_STEP_SIZE);
        if (lite_adc.xfer && !xQueueSendToBackFromISR(lite_adc.xque, &recv, &yield)) {
            CLOG("AADC:LOSE");
            xQueueReset(lite_adc.xque);
        }
    }
    if (event & CSK_ADCPDM_EVENT_RX_FIFO_FULL) CLOG("AADC:RXF"); 
    if (event & CSK_ADCPDM_EVENT_RX_FIFO_OVERRUN) CLOG("AADC:OVR");

    portYIELD_FROM_ISR(yield);
    return;
}

int lite_adc_available(void)
{
    return uxQueueMessagesWaiting(lite_adc.xque) * AADC_STEP_SAMP;
}

int lite_adc_read(void *dst, int size, TickType_t msec)
{
    return xQueueReceive(lite_adc.xque, dst, msec) ? AADC_STEP_SAMP : 0;
}

int lite_adc_ctrl(uint32_t uarg, void *parg)
{
    int ret = CSK_DRIVER_OK;
    switch (uarg) {
    case MAPI_AADC_CTRL_REC_START:
        xQueueReset(lite_adc.xque);
        lite_adc.xpos = 0;
        for (int i = 0; i < AADC_RECV_CNTS; i++)
            lite_adc.fifo[i] = heap_caps_aligned_alloc(32, AADC_STEP_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
        CLOG("AADC:START");
        ret = ADC_PDM_Control(lite_adc.hdrv
        #if AADC_DEV_BMP == CH_BMP_STEREO
            , CSK_ADCPDM_RXCFG_MIXED
        #else
            , CSK_ADCPDM_RXCFG_SEPA
        #endif
            , 0);
        REC_ASSERT(0==ret, "ADC_PDM_Control CSK_ADCPDM_RXCFG_SEPA failed");

    #if CONFIG_AADC_PIPO
        ret = ADC_PDM_Receive_PiPo(lite_adc.hdrv, (PIPO_IN_BLOCK[]){
            { .sample_data = lite_adc.fifo[0], .sample_cnt = AADC_STEP_SAMP, .flags = 0 },
            { .sample_data = lite_adc.fifo[1], .sample_cnt = AADC_STEP_SAMP, .flags = 0 },
        }, &(uint8_t){2}, AADC_DEV_BMP, ADC_PDM_RX_FLAG_START_NOW);
    #else
        ret = ADC_PDM_Receive(lite_adc.hdrv, lite_adc.fifo[0], AADC_STEP_SAMP, AADC_DEV_BMP, ADC_PDM_RX_FLAG_START_NOW);
    #endif
        CLOG("[%d]lite_adc ret:%d", __LINE__, ret);
        lite_adc.xfer = true;
        break;
    case MAPI_AADC_CTRL_REC_STOP:
        CLOG("AADC:STOP\r");
        lite_adc.xfer = false;
        ADC_PDM_Abort(lite_adc.hdrv, AADC_DEV_BMP);
        for (int i = 0; i < AADC_RECV_CNTS; i++) {
            heap_caps_free(lite_adc.fifo[i]);
            lite_adc.fifo[i] = NULL;
        }
        lite_adc.xpos = 0;
        break;
    case MAPI_AADC_CTRL_REC_RESUME:
        CLOG("AADC:RESUME");
        if (!lite_adc.xfer) {
            xQueueReset(lite_adc.xque);
            lite_adc.xfer = true;
        }
        break;
    case MAPI_AADC_CTRL_REC_PAUSE:
        CLOG("AADC:PAUSE");
        lite_adc.xfer = false;
        break;
    case MAPI_AADC_CTRL_REC_RESET:
        CLOG("AADC:RESET");
        xQueueReset(lite_adc.xque);
        break;
    case MAPI_AADC_CTRL_GET_SAMPS: {
        int *const samp = parg;
        *samp = CONFIG_AUDIO_STEP_SAMPS;
        CLOG("AADC:STEPSAMP=%d", *samp);
        break;
    }
    case MAPI_AADC_CTRL_SET_GAIN: { // set gain, val[0]:agin, step:2
        struct { int al, ar, dl, dr; } *gain = parg;
        uint32_t gain_a = 0, gain_d = 0, vol_flag = 0;

    #if AADC_DEV_BMP & CH_BMP_LEFT
        if (gain->al > ADC_PDM_GAIN_A_MAX_DB) gain->al = ADC_PDM_GAIN_A_MAX_DB;
        if (gain->al < ADC_PDM_GAIN_A_MIN_DB) gain->al = ADC_PDM_GAIN_A_MIN_DB;
        if (gain->dl > ADC_PDM_GAIN_D_MAX_DB) gain->dl = ADC_PDM_GAIN_D_MAX_DB;
        if (gain->dl < ADC_PDM_GAIN_D_MIN_DB) gain->dl = ADC_PDM_GAIN_D_MIN_DB;

        gain_a |= ADC_PDM_GAIN_A_VAL(gain->al);
        gain_d |= ADC_PDM_GAIN_D_VAL(gain->dl);
        vol_flag |= ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_D_LEFT;

        CLOG("AADC.L: ANA=%ddB DIG=%ddB", gain->al, gain->dl);
    #endif
    #if AADC_DEV_BMP & CH_BMP_RIGHT
        if (gain->ar > ADC_PDM_GAIN_A_MAX_DB) gain->ar = ADC_PDM_GAIN_A_MAX_DB;
        if (gain->ar < ADC_PDM_GAIN_A_MIN_DB) gain->ar = ADC_PDM_GAIN_A_MIN_DB;
        if (gain->dr > ADC_PDM_GAIN_D_MAX_DB) gain->dr = ADC_PDM_GAIN_D_MAX_DB;
        if (gain->dr < ADC_PDM_GAIN_D_MIN_DB) gain->dr = ADC_PDM_GAIN_D_MIN_DB;

        gain_a |= ADC_PDM_GAIN_A_VAL(gain->ar) << 16;
        gain_d |= ADC_PDM_GAIN_D_VAL(gain->dr) << 16;
        vol_flag |= ADC_PDM_VOL_FLAG_A_RIGHT | ADC_PDM_VOL_FLAG_D_RIGHT;

        CLOG("AADC.R: ANA=%ddB DIG=%ddB", gain->ar, gain->dr);
    #endif
        CLOG("AADC ANA=0x%x DIG=0x%x, flag=0x%x", gain_a, gain_d, vol_flag);
        ret |= ADC_PDM_SetVolume(lite_adc.hdrv, gain_a, gain_d, vol_flag);
        ASSERT(CSK_DRIVER_OK == ret, "setvol(%d)", ret);

        break;
    }
    case MAPI_AADC_CTRL_SET_HPF: {
        uint32_t *param = (uint32_t *)parg;
        CLOG("AADC:HPF1:%lu, HPF2:%lu, HPF2CUT:%lu", param[0], param[1], param[2]);
        ret = ADC_PDM_Control(lite_adc.hdrv, CSK_ADCPDM_HPF_SET, param[0]| param[1]| param[2]);
        break;
    }
    default:
        return -1;
    }
    return 0;
}

int lite_adc_init(void)
{
    CLOG("AADC:INIT enter");
    lite_adc.hdrv = ADC_PDM01();
    lite_adc.xque = xQueueCreate(AADC_RECV_CNTS-2, sizeof(void *));

    // mono(left channel) mic + echo?, 16bits
    ADC_PDM_DMA_CHS dmach = {
    #if AADC_DEV_BMP & CH_BMP_LEFT
        .dma_ch_in_left = GPDMA_ADC0_CHN,
    #else
        .dma_ch_in_left = DMA_CHANNEL_ANY,
    #endif
    #if AADC_DEV_BMP & CH_BMP_RIGHT
        .dma_ch_in_right = GPDMA_ADC1_CHN
    #else
        .dma_ch_in_right = DMA_CHANNEL_ANY
    #endif
    };
    int ret = ADC_PDM_Initialize(lite_adc.hdrv, aadc_drv_event, 0
        , ADC_PDM_BMP_FLAG_USE_16BITS | AADC_DEV_BMP
    #if CONFIG_AADC_DMIC
        | ADC_PDM_BMP_FLAG_USE_PDM
    #endif
        , &dmach);
    REC_ASSERT(0==ret, goto EXIT);
    REC_ASSERT((ret = ADC_PDM_PowerControl(lite_adc.hdrv, CSK_POWER_FULL)) == 0, goto EXIT);
    REC_ASSERT((ret = ADC_PDM_Control(lite_adc.hdrv, CSK_ADCPDM_SR_16KHZ | CSK_ADCPDM_OSR_250, 0)) == 0, goto EXIT);
    ret = ADC_PDM_Control(lite_adc.hdrv
    #if AADC_DEV_BMP == CH_BMP_STEREO
        , CSK_ADCPDM_RXCFG_MIXED
    #else
        , CSK_ADCPDM_RXCFG_SEPA
    #endif
        , 0);
    REC_ASSERT(0==ret, goto EXIT);

#if CONFIG_AADC_HPF
    ret = ADC_PDM_Control(lite_adc.hdrv, CSK_ADCPDM_HPF_SET
        , CSK_ADCPDM_ARG_HPF1_EN | CSK_ADCPDM_ARG_HPF2_EN | CSK_ADCPDM_ARG_HPF2_CUT(3));
    REC_ASSERT(0==ret, goto EXIT);
#endif

    ret = ADC_PDM_Control(lite_adc.hdrv, CSK_ADCPDM_PGA_INPUT_SET
    #if CONFIG_AADC_DIFF
        , CSK_ADCPDM_ARG_LPGA_INPUT_DIFFER | CSK_ADCPDM_ARG_RPGA_INPUT_DIFFER
    #else
        , CSK_ADCPDM_ARG_LPGA_INPUT_SINGLE | CSK_ADCPDM_ARG_RPGA_INPUT_SINGLE
    #endif
    );
    REC_ASSERT(0==ret, goto EXIT);
    // volume: analog=-12~+36db, digital=0db
    ret = ADC_PDM_SetVolume(lite_adc.hdrv
        , ADC_PDM_GAIN_A_VAL(CONFIG_AADC_GAIN_A)
        , ADC_PDM_GAIN_D_VAL(CONFIG_AADC_GAIN_D)
        , ADC_PDM_VOL_FLAG_A_LEFT | ADC_PDM_VOL_FLAG_A_RIGHT 
        | ADC_PDM_VOL_FLAG_D_LEFT | ADC_PDM_VOL_FLAG_D_RIGHT
    );
    REC_ASSERT(0==ret, goto EXIT);
    // unmute at first
    REC_ASSERT((ret = ADC_PDM_SetMute(lite_adc.hdrv, 0, AADC_DEV_BMP)) == 0, goto EXIT);

    //gpio init
    #if AADC_DEV_BMP & CH_BMP_RIGHT
    /* mic1的引脚配置 */
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_FRC  = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_OEN_REG  = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_FRC   = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_IE_REG   = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_FRC = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_UP  = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_PULL_DN  = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_FRC  = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_OEN_REG  = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_FRC   = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_IE_REG   = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_FRC = 1;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_UP  = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_PULL_DN  = 0;

    IP_CMN_IOMUX->REG_PAD_GPIOA_28.bit.PAD_GPIOA_28_FSEL = 21; // MIC1_INP
    IP_CMN_IOMUX->REG_PAD_GPIOA_29.bit.PAD_GPIOA_29_FSEL = 21; // MIC1_INN
    #endif

    #if AADC_DEV_BMP & CH_BMP_LEFT
    /* mic0的引脚配置 */
    IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_FRC  = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_OEN_REG  = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_FRC   = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_IE_REG   = 0;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_FRC = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_UP  = 0;
	IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_PULL_DN  = 0;
    IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_FRC  = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_OEN_REG  = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_FRC   = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_IE_REG   = 0;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_FRC = 1;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_UP  = 0;
	IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_PULL_DN  = 0;

    IP_CMN_IOMUX->REG_PAD_GPIOA_30.bit.PAD_GPIOA_30_FSEL = 21; // MIC0_INP
    IP_CMN_IOMUX->REG_PAD_GPIOA_31.bit.PAD_GPIOA_31_FSEL = 21; // MIC0_INN
    #endif
    IP_AON_CTRL->REG_AON_TUNE1.bit.TUNE_LDOVA = 0x7;
    
EXIT:

    CLOG("AADC:INIT exit");
    return ret;
}

int lite_adc_deinit(void){
    int ret;

    if(lite_adc.xfer){
        lite_adc_ctrl(MAPI_AADC_CTRL_REC_STOP, 0);
    }
    vQueueDelete(lite_adc.xque);
    REC_ASSERT((ret = ADC_PDM_Uninitialize(lite_adc.hdrv)) == 0, goto EXIT);
EXIT:
    return ret;
}
