#include <string.h>
#include "log_print.h"
#include "systick.h"
#include "Driver_DAC.h"
#include "Driver_ADC_PDM.h"
#include "Driver_GPIO.h"
#include "Driver_Common.h"
#include "Driver_GPDMA.h"
#include "IOMuxManager.h"

#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "stream_buffer.h"
#include "esp_heap_caps_init.h"
#include "cache.h"
#include "lite_dac.h"

#define TAG "DAC"
#include "lisa_log.h"
#define ADAC_EVT_DONE     (1<<0)

#define ADAC_SAMPLE_BYTE  (2)
#define ADAC_QUE_BUF_SIZE (256)

#define ADAC_SEND_CNTS    (12) //
#define GPDMA_DAC0_CHN    (2)
#define GPDMA_ECHO_CHN    (3)
#define ADAC_DEV_BMP      (DAC_BMP_LEFT)

#define DAC_ECHO_ENABLE (CONFIG_DAC_ECHO_ENABLE)
#if DAC_ECHO_ENABLE
#define ECHO_RECV_CNTS      (CONFIG_ECHO_RECV_CNTS)
#define ECHO_STEP_SAMP      (CONFIG_AUDIO_STEP_SAMPS )
#define ECHO_STEP_SIZE      (sizeof(short) * ECHO_STEP_SAMP)
#endif


#define ADAC_DEV_AGAIN    (-18)
#define ADAC_DEV_DGAIN    (-1)

#define ADAC_PA_T_STA     (500)
#define ADAC_PA_T_HL      (50)

#define PA_DEV       GPIOA()
#define PA_PAD       (CSK_IOMUX_PAD_A)
#define PA_PIN_NUM   (28)
#define PA_PIN       (CSK_GPIO_PIN28)
#define PA_DEBOUNCE  (CSK_GPIO_DEBOUNCE_DISABLE)
#define PA_MODE_DEFAULT_PULSE (4)

#define PLAY_ASSERT(cond, code)                \
	do {                                                \
		if (!(cond)) {                                  \
			CLOG("[%s %d]"#cond"\r", __FILE__, __LINE__); \
			code;                                       \
		}                                               \
	} while (0)

typedef struct {
    void *addr;
    int samp;
} dac_item_t;

static struct
{
    void* hdrv;
    void* ping_addr;
    void* pong_addr;
    uint32_t *zero;
    int sr;
    int osr;
    uint8_t *buf;
    QueueHandle_t xque;

    #if DAC_ECHO_ENABLE
	int echo_xpos;
	QueueHandle_t echo_xque;
	void *echo_fifo[ECHO_RECV_CNTS];
    #endif
    QueueHandle_t xque_buf;
    EventGroupHandle_t xevt;
    enum { ADAC_STAT_IDLE, ADAC_STAT_PLAY_REQ, ADAC_STAT_PLAY_RUN, ADAC_STAT_STOP_REQ } stat;
    int a_gain;
    int d_gain;
}lite_dac = {
    .a_gain = ADAC_DEV_AGAIN,
    .d_gain = ADAC_DEV_DGAIN,
};

static void dac_drv_event(uint32_t event, uint32_t user);

static int g_pa_pulse = PA_MODE_DEFAULT_PULSE;

void dac_pa_ctrl(int enable)
{
	IOMuxManager_PinConfigure(PA_PAD, PA_PIN_NUM, CSK_IOMUX_FUNC_DEFAULT);
	GPIO_Initialize(PA_DEV, NULL, NULL);
	GPIO_Control(PA_DEV, PA_DEBOUNCE, PA_PIN);
	GPIO_SetDir(PA_DEV, PA_PIN, CSK_GPIO_DIR_OUTPUT);

    if(enable){
        for(volatile int i = 0; i < g_pa_pulse; i++) {
            GPIO_PinWrite(PA_DEV, PA_PIN, 0);
            SysTick_Delay_Us(ADAC_PA_T_HL);
            GPIO_PinWrite(PA_DEV, PA_PIN, 1);
            SysTick_Delay_Us(ADAC_PA_T_HL);
        }
    }else{
        GPIO_PinWrite(PA_DEV, PA_PIN, 0);
        SysTick_Delay_Us(ADAC_PA_T_HL);
    }
}
void lite_dac_pa_pulse_set(int pulse)
{
    g_pa_pulse = pulse;
    g_pa_pulse = PA_MODE_DEFAULT_PULSE;
    CLOG("pa set pulse %d", g_pa_pulse);
}

//dac兜底，重启恢复，此操作若是经常出现，系统有问题
static int dac_reset(void){
    int ret;

    ret = DAC_Uninitialize(lite_dac.hdrv);
    ret |= DAC_Initialize(lite_dac.hdrv, dac_drv_event, (uint32_t)0
    , (ADAC_DEV_BMP << DAC_BMP_FLAG_OUT_POS) | DAC_BMP_FLAG_USE_16BITS
    , &(DAC_DMA_CHS){ .dma_ch_out_left = GPDMA_DAC0_CHN, .dma_ch_echo_left = GPDMA_ECHO_CHN });
    ret |= DAC_PowerControl(lite_dac.hdrv, CSK_POWER_FULL);

    ret |= DAC_Control(lite_dac.hdrv
        , lite_dac.sr | lite_dac.osr | CSK_DAC_SOFT_MUTE_SET
        , CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(3));
    ret |= DAC_SetMute(lite_dac.hdrv, ADAC_DEV_BMP, ADAC_DEV_BMP);

    ret |= DAC_SetVolume(lite_dac.hdrv
        , DAC_GAIN_A_VAL(lite_dac.a_gain), DAC_GAIN_D_VAL(lite_dac.d_gain)
        , DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT);

    PIPO_OUT_BLOCK pipo[] = {
        [0] = { .sample_data = lite_dac.zero, .sample_cnt = ADAC_QUE_BUF_SIZE, .flags = 0 },
        [1] = { .sample_data = lite_dac.zero, .sample_cnt = ADAC_QUE_BUF_SIZE, .flags = 0 },
    };
    lite_dac.ping_addr = lite_dac.zero;
    lite_dac.pong_addr = lite_dac.zero;
    ret |= DAC_Send_PiPo(lite_dac.hdrv, pipo, &(uint8_t){2}, ADAC_DEV_BMP, DAC_TX_FLAG_START_NOW);
    DAC_SetMute(lite_dac.hdrv, 0, ADAC_DEV_BMP);      
    dac_pa_ctrl(ADAC_PA_OPEN);

    return ret;
}

static int dac_buf_init(void){

    lite_dac.buf = heap_caps_aligned_alloc(32, ADAC_SEND_CNTS*ADAC_SAMPLE_BYTE*ADAC_QUE_BUF_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if(NULL == lite_dac.buf){

        CLOGE("[DAC] no enough buf");
        return -1;
    }

    for(int i=0;i<ADAC_SEND_CNTS;i++){
        dac_item_t item = {
            .addr = lite_dac.buf+i*ADAC_SAMPLE_BYTE*ADAC_QUE_BUF_SIZE,
            .samp = 0,
        } ;
        // CLOG("%p", item.addr);
        xQueueSendToBack(lite_dac.xque_buf, &item, 0);
    }
    return 0;
}

static void dac_drv_event(uint32_t event, uint32_t user){
    BaseType_t yield = pdFALSE;

    if (event & CSK_DAC_EVENT_SEND_COMPLETE) {
        lite_dac.stat = ADAC_STAT_IDLE;
        xEventGroupSetBitsFromISR(lite_dac.xevt, ADAC_EVT_DONE, &yield);
    }

#if DAC_ECHO_ENABLE
	if (event & (CSK_ADCPDM_EVENT_RECEIVE_COMPLETE | CSK_ADCPDM_EVENT_BLOCK_COMPLETE)) {
		int ret = CSK_DRIVER_OK;
        void *recv = lite_dac.echo_fifo[lite_dac.echo_xpos];
        if (++lite_dac.echo_xpos >= ECHO_RECV_CNTS) lite_dac.echo_xpos = 0;
        int ipos = lite_dac.echo_xpos;
        if (++ipos >= ECHO_RECV_CNTS) ipos = 0;

        ret = DAC_Echo_Receive_PiPo(lite_dac.hdrv
            , &(PIPO_IN_BLOCK){ .sample_data = lite_dac.echo_fifo[ipos], .sample_cnt = ECHO_STEP_SAMP, .flags = 0 }
            , &(uint8_t){1}, ADAC_DEV_BMP);
         if(CSK_DRIVER_OK != ret){
            CLOGE("DAC_Echo_Receive_PiPo:%d", ret);
         }

        if (!xQueueSendFromISR(lite_dac.echo_xque, &recv, &yield)) {
            CLOGW("ECHO:LOSE");
            xQueueReset(lite_dac.echo_xque);
        }
    }
#endif

    if (event & (CSK_DAC_EVENT_SEND_COMPLETE |
                 CSK_DAC_EVENT_BLOCK_COMPLETE)) {

        int ret = CSK_DRIVER_OK;
        uint32_t* addr = lite_dac.zero;
        dac_item_t item = { .addr = lite_dac.zero, .samp = ADAC_QUE_BUF_SIZE };
        switch (lite_dac.stat) {
        case ADAC_STAT_PLAY_REQ:
            dac_pa_ctrl(ADAC_PA_OPEN);
            ret = DAC_SetMute(lite_dac.hdrv, 0, ADAC_DEV_BMP);
            if(CSK_DRIVER_OK != ret){
                CLOGE("DAC_SetMute:%d", ret);
            }
            lite_dac.stat = ADAC_STAT_PLAY_RUN; 
            // @suppress("No break at end of case")
        case ADAC_STAT_PLAY_RUN: {
            xQueueReceiveFromISR(lite_dac.xque, &item, &yield);
            ret = DAC_Send_PiPo(lite_dac.hdrv
                , &(PIPO_OUT_BLOCK){ .sample_data = item.addr, .sample_cnt = item.samp, .flags = 0 }
                , &(uint8_t){1}, ADAC_DEV_BMP, DAC_TX_FLAG_START_NOW);

            if(CSK_DRIVER_OK != ret){
                CLOGE("DAC_Send_PiPo:%d", ret);
            }
            // CLOG("play:%p size:%d", item.addr, item.samp);
            break;
        }
        case ADAC_STAT_STOP_REQ:
            ret = DAC_SetMute(lite_dac.hdrv, ADAC_DEV_BMP, ADAC_DEV_BMP);
            if(CSK_DRIVER_OK != ret){
                CLOGE("DAC_SetMute:%d", ret);
            }
            dac_pa_ctrl(ADAC_PA_CLOSE);
            ret = DAC_Abort(lite_dac.hdrv, ADAC_DEV_BMP, 0);
            if(CSK_DRIVER_OK != ret){
                CLOGE("DAC_Abort:%d", ret);
            }
            lite_dac.stat = ADAC_STAT_IDLE;
            xEventGroupSetBitsFromISR(lite_dac.xevt, ADAC_EVT_DONE, &yield);
            break;
        case ADAC_STAT_IDLE:
            CLOGW("DAC:IDLE(%#lx)", event);
            break;
        }
        if(event & CSK_DAC_EVENT_TX_PING_DONE){
            addr = lite_dac.ping_addr;
            lite_dac.ping_addr = item.addr;
        }else if(event & CSK_DAC_EVENT_TX_PONG_DONE){
            addr = lite_dac.pong_addr;
            lite_dac.pong_addr = item.addr;
        }else{
            addr = item.addr;
        }
        if(addr!=lite_dac.zero){
            dac_item_t item = {.addr=addr, .samp=0};
            xQueueSendToBackFromISR(lite_dac.xque_buf, &item, &yield);
        }
    } else {
        // CLOGW("DAC:EVT=%#lx", event);
    }
    if (event & CSK_DAC_EVENT_TX_FIFO_EMPTY) CLOGW("DAC:TXE");
    if (event & CSK_DAC_EVENT_TX_FIFO_UNDERRUN) {
        CLOGW("[DAC:UDR]restart :%d stat:%d", dac_reset(), lite_dac.stat);       
    }
    // CLOG("0x%x %d", event, lite_dac.stat);
    portYIELD_FROM_ISR(yield);
    return;
}
//
int lite_dac_get_buf(uint8_t **buf, TickType_t xTicksToWait)
{
    dac_item_t item;
    return xQueueReceive(lite_dac.xque_buf, &item, xTicksToWait) == pdPASS ?\
                        (*buf = item.addr, ADAC_QUE_BUF_SIZE) : (*buf = NULL, 0);
}

int lite_dac_get_echo_buf(uint16_t **buf, TickType_t xTicksToWait)
{
	#if DAC_ECHO_ENABLE
    dac_item_t item;
    return xQueueReceive(lite_dac.echo_xque, &item, xTicksToWait) == pdPASS ?\
                        (*buf = item.addr, ECHO_STEP_SAMP) : (*buf = NULL, 0);
	#else
	*buf = NULL;
	return 0;
	#endif
}

int lite_dac_write(void *src, int size, TickType_t xTicksToWait)
{
    dac_item_t item = {.addr=src, .samp=size};
    HAL_FlushDCache_by_Addr(src, ADAC_SAMPLE_BYTE*ADAC_QUE_BUF_SIZE);
    return xQueueSendToBack(lite_dac.xque, &item, xTicksToWait) ? size : 0;
}

bool lite_dac_queue_empty(void)
{
   return (uxQueueMessagesWaiting(lite_dac.xque) == 0);
}

int lite_dac_left_sample(void)
{
    int sample = 0;
    int qnum = uxQueueMessagesWaiting(lite_dac.xque);
    if(qnum){
        dac_item_t item;
        if(xQueuePeek(lite_dac.xque, &item, 0)==pdPASS){
            sample = qnum*item.samp;
        }
    }

    return sample;
}

int lite_dac_ctrl(uint32_t uarg, void *parg)
{
    int ret = CSK_DRIVER_OK;
    // CLOG("lite_dac_ctrl, uarg:%d parg:%d", uarg, parg);
    switch (uarg) {
    case ADAC_CTRL_START:
        if (lite_dac.stat == ADAC_STAT_IDLE) {
            lite_dac.stat = ADAC_STAT_PLAY_REQ;

            PIPO_OUT_BLOCK pipo[] = {
                [0] = { .sample_data = lite_dac.zero, .sample_cnt = ADAC_QUE_BUF_SIZE, .flags = 0 },
                [1] = { .sample_data = lite_dac.zero, .sample_cnt = ADAC_QUE_BUF_SIZE, .flags = 0  },
            };
            lite_dac.ping_addr = lite_dac.zero;
            lite_dac.pong_addr = lite_dac.zero;
            ret = DAC_Send_PiPo(lite_dac.hdrv, pipo, &(uint8_t){2}, ADAC_DEV_BMP, DAC_TX_FLAG_START_NOW);
            CLOG("DAC_Send_PiPo:%d", ret);
            PLAY_ASSERT(0 == ret, asm("nop"));

            #if DAC_ECHO_ENABLE
            ret = DAC_Echo_Receive_PiPo(lite_dac.hdrv, (PIPO_IN_BLOCK[]){
                { .sample_data = lite_dac.echo_fifo[0], .sample_cnt = ECHO_STEP_SAMP, .flags = 0 },
                { .sample_data = lite_dac.echo_fifo[1], .sample_cnt = ECHO_STEP_SAMP, .flags = 0 },
            }, &(uint8_t){2}, ADAC_DEV_BMP);
            CLOG("DAC_Echo_Receive_PiPo:%d", ret);
            PLAY_ASSERT(0 == ret, asm("nop"));
            #endif
        }
        break;
    case ADAC_CTRL_STOP:
        if (lite_dac.stat == ADAC_STAT_PLAY_RUN) {
            lite_dac.stat = ADAC_STAT_STOP_REQ;
            xEventGroupWaitBits(lite_dac.xevt, ADAC_EVT_DONE, true, false, portMAX_DELAY);
            dac_item_t item;
            while(xQueueReceive(lite_dac.xque, &item, 0) == pdPASS){
                item.samp = 0;
                xQueueSendToBack(lite_dac.xque_buf, &item, 0);
            }
            if(lite_dac.ping_addr!=lite_dac.zero){
                item.addr = lite_dac.ping_addr;
                item.samp = 0;
                xQueueSendToBack(lite_dac.xque_buf, &item, 0);
            }
            if(lite_dac.pong_addr!=lite_dac.zero){
                item.addr = lite_dac.pong_addr;
                item.samp = 0;
                xQueueSendToBack(lite_dac.xque_buf, &item, 0);
            }
        }
        break;
    case ADAC_CTRL_VOLUME:
        PLAY_ASSERT(parg, ret=-1;goto EXIT);
        dac_gain_t *gain = (dac_gain_t *)parg;
        CLOG("a gain:%d d gain:%d", gain->a_gain, gain->d_gain);
        lite_dac.a_gain = gain->a_gain;
        lite_dac.d_gain = gain->d_gain;

        ret = DAC_SetVolume(lite_dac.hdrv
            , DAC_GAIN_A_VAL(gain->a_gain), DAC_GAIN_D_VAL(gain->d_gain)
            , DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT);
        PLAY_ASSERT(0 == ret, asm("nop"));
        break;
    case ADAC_CTRL_AUD_CFG:
        PLAY_ASSERT(parg, ret=-1;goto EXIT);
        int sr, osr;
        dac_aud_t *dac_aud = (dac_aud_t *)parg;
        switch(dac_aud->rate){
            case 8000:
                sr = CSK_DAC_SR_8KHZ;
                osr = CSK_DAC_OSR_250;
                break;
            case 16000:
                sr = CSK_DAC_SR_16KHZ;
                osr = CSK_DAC_OSR_250;
                break;
            case 24000:
                sr = CSK_DAC_SR_24KHZ;
                osr = CSK_DAC_OSR_250;
                break;
            case 32000:
                sr = CSK_DAC_SR_32KHZ;
                osr = CSK_DAC_OSR_125;
                break;
            case 48000:
                sr = CSK_DAC_SR_48KHZ;
                osr = CSK_DAC_OSR_125;
                break;
            case 96000:
                sr = CSK_DAC_SR_96KHZ;
                osr = CSK_DAC_OSR_125;
                break;
            default:
                CLOGE("unsupport rate:%d", dac_aud->rate);
                return -1;
                break;
        }
        lite_dac.sr = sr;
        lite_dac.osr = osr;
        ret = DAC_Control(lite_dac.hdrv
            , sr | osr | CSK_DAC_SOFT_MUTE_SET
            , CSK_DAC_ARG_SOFT_MUTE_EN | CSK_DAC_ARG_SOFT_MUTE_SPD(3));
        if(ret != 0) {
            CLOG("DAC ctrl fail(%d), and reset!!!", ret);
            dac_reset();
            ret = 0;
            break;
        }

        #if DAC_ECHO_ENABLE
        ECHO_PARAMS echo_params = { 0 };
        echo_params.echo_mixed = 0; //1; // only 1 ECHO channel for only 1 DAC channel
        echo_params.samp_rate = dac_aud->rate;
        echo_params.trim_16bits = 1; // 16bits echo?
        ret = DAC_Control(lite_dac.hdrv, CSK_DAC_SET_ECHO_PARAMS, (uint32_t)&echo_params);
        if(CSK_DRIVER_OK != ret){
            CLOGE("DAC_Control:%d", ret);
            assert(0);
        }
        #endif

        PLAY_ASSERT(((ret = DAC_SetMute(lite_dac.hdrv, ADAC_DEV_BMP, ADAC_DEV_BMP)) == 0), goto EXIT);
        CLOG("DAC again:%ddB, dgain:%ddB", lite_dac.a_gain, lite_dac.d_gain);
        ret = DAC_SetVolume(lite_dac.hdrv
            , DAC_GAIN_A_VAL(lite_dac.a_gain), DAC_GAIN_D_VAL(lite_dac.d_gain)
            , DAC_VOL_FLAG_A_LEFT | DAC_VOL_FLAG_D_LEFT);
        PLAY_ASSERT(0 == ret, asm("nop"));
        break;
    default:
        return -1;
    }

EXIT:
    return ret;
}

int lite_dac_init(void){
    int ret = -1;

    lite_dac.hdrv = DAC01();
    lite_dac.stat = ADAC_STAT_IDLE;
    lite_dac.xque = xQueueCreate(ADAC_SEND_CNTS, sizeof(dac_item_t));
    lite_dac.xque_buf = xQueueCreate(ADAC_SEND_CNTS, sizeof(dac_item_t));
    lite_dac.xevt = xEventGroupCreate();
    lite_dac.zero = heap_caps_aligned_alloc(32, ADAC_QUE_BUF_SIZE*2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);

    #if DAC_ECHO_ENABLE
    lite_dac.echo_xque =  xQueueCreate(ECHO_RECV_CNTS - 2, sizeof(dac_item_t)); 
    //echo 
	lite_dac.echo_xpos = 0;
    for (int i = 0; i < ECHO_RECV_CNTS; i++) {
    	// lite_dac.echo_fifo[i] = exram_malloc(32, ECHO_STEP_SIZE);
    	lite_dac.echo_fifo[i] = heap_caps_aligned_alloc(32, ECHO_STEP_SIZE, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    }
    #endif

    memset(lite_dac.zero, 0, ADAC_QUE_BUF_SIZE*2);
    CLOG("lite_dac.zero:%p", lite_dac.zero);
    PLAY_ASSERT((ret = dac_buf_init()) == 0, goto ERR);

    dac_pa_ctrl(ADAC_PA_CLOSE);
    #if DAC_ECHO_ENABLE
    ret = DAC_Initialize(lite_dac.hdrv, dac_drv_event, (uint32_t)0
        , (ADAC_DEV_BMP << DAC_BMP_FLAG_OUT_POS) | (ADAC_DEV_BMP << DAC_BMP_FLAG_ECHO_POS) | DAC_BMP_FLAG_USE_16BITS
        , &(DAC_DMA_CHS){ .dma_ch_out_left = GPDMA_DAC0_CHN, .dma_ch_echo_left = GPDMA_ECHO_CHN });
    #else
    ret = DAC_Initialize(lite_dac.hdrv, dac_drv_event, (uint32_t)0
        , (ADAC_DEV_BMP << DAC_BMP_FLAG_OUT_POS) | DAC_BMP_FLAG_USE_16BITS
        , &(DAC_DMA_CHS){ .dma_ch_out_left = GPDMA_DAC0_CHN, .dma_ch_echo_left = GPDMA_ECHO_CHN });
    #endif
    PLAY_ASSERT(0 == ret, goto ERR);
    PLAY_ASSERT(((ret = DAC_PowerControl(lite_dac.hdrv, CSK_POWER_FULL)) == 0), goto ERR);
    return ret;
ERR:
    CLOGE("lite_dac_init:%d\r", ret);
    if(lite_dac.zero){
        heap_caps_free(lite_dac.zero);
        lite_dac.zero = NULL;
    }
    if(lite_dac.buf){
        heap_caps_free(lite_dac.buf);
        lite_dac.buf = NULL;
    }
    vQueueDelete(lite_dac.xque);
    vQueueDelete(lite_dac.xque_buf);
    #if DAC_ECHO_ENABLE
    vQueueDelete(lite_dac.echo_xque);
	lite_dac.echo_xpos = 0;
    for (int i = 0; i < ECHO_RECV_CNTS; i++) {
        if(lite_dac.echo_fifo[i]){
            heap_caps_free(lite_dac.echo_fifo[i]);
            lite_dac.echo_fifo[i] = NULL;
        }
    }
    #endif
    vEventGroupDelete(lite_dac.xevt);
    DAC_Uninitialize(lite_dac.hdrv);
    return ret;
}

int lite_dac_deinit(void){
    int ret;

    dac_pa_ctrl(ADAC_PA_CLOSE);
    PLAY_ASSERT(((ret = DAC_Uninitialize(lite_dac.hdrv)) == 0), goto EXIT);
    vQueueDelete(lite_dac.xque);
    vQueueDelete(lite_dac.xque_buf);
    vEventGroupDelete(lite_dac.xevt);
    if(lite_dac.zero){
        heap_caps_free(lite_dac.zero);
        lite_dac.zero = NULL;
    }
    if(lite_dac.buf){
        heap_caps_free(lite_dac.buf);
        lite_dac.buf = NULL;
    }
    #if DAC_ECHO_ENABLE
    vQueueDelete(lite_dac.echo_xque);
	lite_dac.echo_xpos = 0;
    for (int i = 0; i < ECHO_RECV_CNTS; i++) {
        if(lite_dac.echo_fifo[i]){
            heap_caps_free(lite_dac.echo_fifo[i]);
            lite_dac.echo_fifo[i] = NULL;
        }
    }
    #endif

EXIT:
    return ret;
}
