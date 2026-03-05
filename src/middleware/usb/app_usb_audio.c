#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tusb.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include "lisa_log.h"

#define USBD_STACK_SIZE (4 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

#define BLINKY_STACK_SIZE configMINIMAL_STACK_SIZE
#define AUDIO_STACK_SIZE  configMINIMAL_STACK_SIZE

#define AUDIO_SAMPLE_RATE 16000

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

// static task
#if configSUPPORT_STATIC_ALLOCATION
StackType_t blinky_stack[BLINKY_STACK_SIZE];
StaticTask_t blinky_taskdef;

StackType_t usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

StackType_t audio_stack[AUDIO_STACK_SIZE];
StaticTask_t audio_taskdef;
#endif

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Audio controls
// Current states
bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];       // +1 for master channel 0
uint16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;

// Range states
audio_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1]; // Volume range state
audio_control_range_4_n_t(1) sampleFreqRng;                                     // Sample frequency range state

#if CFG_TUD_AUDIO_ENABLE_ENCODING
// Audio test data, each buffer contains 2 channels, buffer[0] for CH0-1, buffer[1] for CH1-2
uint16_t i2s_dummy_buffer[CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO][CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX *
                                                                  CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 1000 /
                                                                  CFG_TUD_AUDIO_FUNC_1_N_TX_SUPP_SW_FIFO];
#else
// Audio test data, 4 channels muxed together, buffer[0] for CH0, buffer[1] for CH1, buffer[2] for CH2, buffer[3] for
// CH3
uint16_t i2s_dummy_buffer[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX * CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 1000];
#endif

void led_blinking_task(void *param);
void usb_device_task(void *param);
void audio_task(void *param);

/*------------- MAIN -------------*/
int app_usb_audio_run(void)
{
    // Init values
    sampFreq = AUDIO_SAMPLE_RATE;
    clkValid = 1;

    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = AUDIO_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bMax = AUDIO_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bRes = 0;

    // Generate dummy data
#if CFG_TUD_AUDIO_ENABLE_ENCODING
    uint16_t *p_buff = i2s_dummy_buffer[0];
    uint16_t dataVal = 0;
    for (uint16_t cnt = 0; cnt < AUDIO_SAMPLE_RATE / 1000; cnt++) {
        // CH0 saw wave
        *p_buff++ = dataVal;
        // CH1 inverted saw wave
        *p_buff++ = 3200 + AUDIO_SAMPLE_RATE / 1000 - dataVal;
        dataVal += 32;
    }
    p_buff = i2s_dummy_buffer[1];
    for (uint16_t cnt = 0; cnt < AUDIO_SAMPLE_RATE / 1000; cnt++) {
        // CH3 square wave
        *p_buff++ = cnt < (AUDIO_SAMPLE_RATE / 1000 / 2) ? 3400 : 5000;
        // CH4 sinus wave
        float t = 2 * 3.1415f * cnt / (AUDIO_SAMPLE_RATE / 1000);
        *p_buff++ = (uint16_t)((int16_t)(sinf(t) * 750) + 6000);
    }
#else
    uint16_t *p_buff = i2s_dummy_buffer;
    uint16_t dataVal = 0;
    for (uint16_t cnt = 0; cnt < AUDIO_SAMPLE_RATE / 1000; cnt++) {
        // CH0 saw wave
        *p_buff++ = dataVal;
        // CH1 inverted saw wave
        *p_buff++ = 3200 + AUDIO_SAMPLE_RATE / 1000 - dataVal;
        dataVal += 32;
        // CH3 square wave
        *p_buff++ = cnt < (AUDIO_SAMPLE_RATE / 1000 / 2) ? 3400 : 5000;
        // CH4 sinus wave
        float t = 2 * 3.1415f * cnt / (AUDIO_SAMPLE_RATE / 1000);
        *p_buff++ = (uint16_t)((int16_t)(sinf(t) * 750) + 6000);
    }
#endif

#if configSUPPORT_STATIC_ALLOCATION
    // Create a task for audio
    xTaskCreateStatic(audio_task, "audio", AUDIO_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, audio_stack,
                      &audio_taskdef);
#else
    xTaskCreate(audio_task, "audio", AUDIO_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
#endif

    return 0;
}

static QueueHandle_t audio_queue;
static bool is_recording = false;

#define TAG "usb_audio"
#include "sysheap.h"
#include "lisa_log.h"

void app_usb_audio_start_recording(void)
{
    is_recording = true;
}

void app_usb_audio_stop_recording(void)
{
    is_recording = false;
}

bool app_usb_audio_is_recording(void)
{
    return is_recording;
}

struct audio_data {
    uint16_t *data;
    uint32_t size;
};

int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit)
{
    struct audio_data audio_data = {
        .size = 4 * sample * sizeof(uint16_t),
    };

    if (audio_queue == NULL) {
        return -1;
    }

    audio_data.data = psram_malloc(audio_data.size);
    if (!audio_data.data) {
        return -1;
    }

    uint16_t(*input)[channel] = (uint16_t(*)[channel])data;
    uint16_t(*output)[4] = (uint16_t(*)[4])audio_data.data;
    for (int i = 0; i < sample; i++) {
        output[i][0] = input[i][0];
        output[i][1] = input[i][1];
        output[i][2] = input[i][2];
        output[i][3] = input[i][3];
    }

    BaseType_t ret = xQueueSend(audio_queue, &audio_data, 0);
    if (ret != pdPASS) {
        psram_free(audio_data.data);
        LISA_LOGE(TAG, "audio queue send failed");
        return -1;
    }

    return 0;
}

void audio_task(void *param)
{
    (void)param;
    audio_queue = xQueueCreate(100, sizeof(struct audio_data));

    while (1) {
        struct audio_data audio_data;
        if (xQueueReceive(audio_queue, &audio_data, portMAX_DELAY) != pdPASS) {
            continue;
        }

        tud_audio_write(audio_data.data, audio_data.size);
        psram_free(audio_data.data);
    }
}

//--------------------------------------------------------------------+
// Application Callback API Implementations
//--------------------------------------------------------------------+

// Invoked when audio class specific set request received for an EP
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;
    (void)pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)ep;

    return false; // Yet not implemented
}

// Invoked when audio class specific set request received for an interface
bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;
    (void)pBuff;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)itf;

    return false; // Yet not implemented
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    (void)itf;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // If request is for our feature unit
    if (entityID == 2) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Request uses format layout 1
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

            mute[channelNum] = ((audio_control_cur_1_t *)pBuff)->bCur;

            LOGI("    Set Mute: %d of channel: %u\r\n", mute[channelNum], channelNum);
            return true;

        case AUDIO_FU_CTRL_VOLUME:
            // Request uses format layout 2
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

            volume[channelNum] = ((audio_control_cur_2_t *)pBuff)->bCur;

            LOGI("    Set Volume: %d dB of channel: %u\r\n", volume[channelNum], channelNum);
            return true;

            // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }
    return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an EP
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t ep = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)ep;

    return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an interface
bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);

    (void)channelNum;
    (void)ctrlSel;
    (void)itf;

    return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since we have only one audio function
    // implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // Input terminal (Microphone input)
    if (entityID == 1) {
        switch (ctrlSel) {
        case AUDIO_TE_CTRL_CONNECTOR: {
            // The terminal connector control only has a get request with only the CUR attribute.
            audio_desc_channel_cluster_t ret;

            // Those are dummy values for now
            ret.bNrChannels = 1;
            ret.bmChannelConfig = 0;
            ret.iChannelNames = 0;

            LOGI("    Get terminal connector\r\n");

            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void *)&ret, sizeof(ret));
        } break;

            // Unknown/Unsupported control selector
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Feature unit
    if (entityID == 2) {
        switch (ctrlSel) {
        case AUDIO_FU_CTRL_MUTE:
            // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
            // There does not exist a range parameter block for mute
            LOGI("    Get Mute of channel: %u\r\n", channelNum);
            return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

        case AUDIO_FU_CTRL_VOLUME:
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                LOGI("    Get Volume of channel: %u\r\n", channelNum);
                return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));

            case AUDIO_CS_REQ_RANGE:
                LOGI("    Get Volume range of channel: %u\r\n", channelNum);

                // Copy values - only for testing - better is version below
                audio_control_range_2_n_t(1) ret;

                ret.wNumSubRanges = 1;
                ret.subrange[0].bMin = -90; // -90 dB
                ret.subrange[0].bMax = 90;  // +90 dB
                ret.subrange[0].bRes = 1;   // 1 dB steps

                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void *)&ret, sizeof(ret));

                // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
            }
            break;

            // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    // Clock Source unit
    if (entityID == 4) {
        switch (ctrlSel) {
        case AUDIO_CS_CTRL_SAM_FREQ:
            // channelNum is always zero in this case
            switch (p_request->bRequest) {
            case AUDIO_CS_REQ_CUR:
                LOGI("    Get Sample Freq.\r\n");
                // Buffered control transfer is needed for IN flow control to work
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));

            case AUDIO_CS_REQ_RANGE:
                LOGI("    Get Sample Freq. range\r\n");
                return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));

                // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
            }
            break;

        case AUDIO_CS_CTRL_CLK_VALID:
            // Only cur attribute exists for this request
            LOGI("    Get Sample Freq. valid\r\n");
            return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

        // Unknown/Unsupported control
        default:
            TU_BREAKPOINT();
            return false;
        }
    }

    LOGI("  Unsupported entity: %d\r\n", entityID);
    return false; // Yet not implemented
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;

    // In read world application data flow is driven by I2S clock,
    // both tud_audio_tx_done_pre_load_cb() & tud_audio_tx_done_post_load_cb() are hardly used.
    // For example in your I2S receive callback:
    // void I2S_Rx_Callback(int channel, const void* data, uint16_t samples)
    // {
    //    tud_audio_write_support_ff(channel, data, samples * N_BYTES_PER_SAMPLE * N_CHANNEL_PER_FIFO);
    // }

    return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t rhport, uint16_t n_bytes_copied, uint8_t itf, uint8_t ep_in,
                                    uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)n_bytes_copied;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;

    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    (void)p_request;

    return true;
}
