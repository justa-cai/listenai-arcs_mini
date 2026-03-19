#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "shell.h"
#include "mbedtls/md5.h"
#include "sysheap.h"

#define TAG "wakeup_debug"
#include "lisa_log.h"

extern int app_usb_audio_write(void *data, uint32_t sample, uint32_t channel, uint8_t bit);
#if CONFIG_APP_USB_CDC_ENABLE
extern int app_usb_cdc_audio_write(uint8_t *data, uint32_t len);
#endif

#if CONFIG_APP_USB_AUDIO_ENABLE
static volatile bool wakeup_uac_record_flag = true;
#else
static volatile bool wakeup_uac_record_flag = false;
#endif


// MD5 context for CDC stream
static mbedtls_md5_context cdc_md5_ctx;
static bool cdc_md5_initialized = false;
static uint64_t cdc_total_bytes = 0;

#define WAKEUP_STREAM_CHANNELS       (5)
#define WAKEUP_STREAM_SAMPLE_CNT     (256)
#define WAKEUP_STREAM_ONE_FRAME_SIZE (WAKEUP_STREAM_SAMPLE_CNT * WAKEUP_STREAM_CHANNELS * sizeof(int16_t))
#define WAKEUP_UAC_CHANNELS          (4)
#define WAKEUP_RAW_RING_SIZE         (32)
#define WAKEUP_RECORD_DUAL_CH_SAMPLES (WAKEUP_STREAM_SAMPLE_CNT * 2U)

typedef struct {
    int16_t mic1;
    int16_t mic2;
    int16_t soft_ref;
} wakeup_raw_sample_t;

typedef struct {
    wakeup_raw_sample_t sample[WAKEUP_STREAM_SAMPLE_CNT];
    uint8_t has_mic2;
    uint8_t has_soft_ref;
} wakeup_raw_frame_t;

enum {
    WAKEUP_UAC_CH_MIC = 0,       /* mic */
    WAKEUP_UAC_CH_HARD_REF = 1,  /* hard ref (from mic2) */
    WAKEUP_UAC_CH_SOFT_REF = 2,  /* soft ref */
    WAKEUP_UAC_CH_ALGO_OUT = 3,  /* algorithm output */
};

static wakeup_raw_frame_t *s_raw_ring = NULL;
static uint32_t s_raw_read_idx = 0;
static uint32_t s_raw_write_idx = 0;
static uint32_t s_raw_count = 0;

static bool wakeup_raw_ring_init(void)
{
    if (s_raw_ring) {
        return true;
    }

    wakeup_raw_frame_t *ring = psram_malloc(sizeof(wakeup_raw_frame_t) * WAKEUP_RAW_RING_SIZE);
    if (!ring) {
        LISA_LOGE(TAG, "alloc wakeup raw ring failed");
        return false;
    }
    memset(ring, 0, sizeof(wakeup_raw_frame_t) * WAKEUP_RAW_RING_SIZE);

    taskENTER_CRITICAL();
    if (!s_raw_ring) {
        s_raw_ring = ring;
    } else {
        psram_free(ring);
    }
    taskEXIT_CRITICAL();

    return (s_raw_ring != NULL);
}

static void wakeup_raw_frame_push(const wakeup_raw_frame_t *frame)
{
    if (!frame) {
        return;
    }

    if (!wakeup_raw_ring_init()) {
        return;
    }

    taskENTER_CRITICAL();

    /* Drop oldest frame when ring buffer is full. */
    if (s_raw_count >= WAKEUP_RAW_RING_SIZE) {
        s_raw_read_idx = (s_raw_read_idx + 1U) % WAKEUP_RAW_RING_SIZE;
        s_raw_count--;
    }

    s_raw_ring[s_raw_write_idx] = *frame;
    s_raw_write_idx = (s_raw_write_idx + 1U) % WAKEUP_RAW_RING_SIZE;
    s_raw_count++;

    taskEXIT_CRITICAL();
}

static bool wakeup_raw_frame_pop(wakeup_raw_frame_t *frame)
{
    bool ok = false;

    if (!frame || !s_raw_ring) {
        return false;
    }

    taskENTER_CRITICAL();
    if (s_raw_count > 0U) {
        *frame = s_raw_ring[s_raw_read_idx];
        s_raw_read_idx = (s_raw_read_idx + 1U) % WAKEUP_RAW_RING_SIZE;
        s_raw_count--;
        ok = true;
    }
    taskEXIT_CRITICAL();

    return ok;
}

void wakeup_stream_debug_data_input(const void *record_buf, const void *echo_buf, uint32_t record_samples,
                                    uint32_t echo_samples)
{
#if CONFIG_APP_USB_AUDIO_ENABLE
    if (!record_buf || record_samples < WAKEUP_STREAM_SAMPLE_CNT) {
        return;
    }

    const int16_t *record = (const int16_t *)record_buf;
    const int16_t *echo = (const int16_t *)echo_buf;
    bool has_dual_record = (record_samples >= WAKEUP_RECORD_DUAL_CH_SAMPLES);

    wakeup_raw_frame_t frame = {0};
    frame.has_mic2 = has_dual_record ? 1U : 0U;
    frame.has_soft_ref = (echo != NULL && echo_samples >= WAKEUP_STREAM_SAMPLE_CNT) ? 1U : 0U;

    for (uint32_t i = 0; i < WAKEUP_STREAM_SAMPLE_CNT; i++) {
        if (has_dual_record) {
            frame.sample[i].mic1 = record[i * 2U];
            frame.sample[i].mic2 = record[(i * 2U) + 1U];
        } else {
            frame.sample[i].mic1 = record[i];
            frame.sample[i].mic2 = 0;
        }
        frame.sample[i].soft_ref = frame.has_soft_ref ? echo[i] : 0;
    }

    wakeup_raw_frame_push(&frame);
#else
    (void)record_buf;
    (void)echo_buf;
    (void)record_samples;
    (void)echo_samples;
#endif
}

void wakeup_stream_debug_data_output(uint8_t *data, uint32_t len)
{
#if CONFIG_APP_USB_AUDIO_ENABLE
    if (wakeup_uac_record_flag && data && len >= WAKEUP_STREAM_ONE_FRAME_SIZE) {
        uint32_t cnt = len / WAKEUP_STREAM_ONE_FRAME_SIZE;
        const int16_t(*algo_frame)[WAKEUP_STREAM_CHANNELS] = NULL;

        for (uint32_t i = 0; i < cnt; i++) {
            wakeup_raw_frame_t raw_frame;
            bool has_raw = wakeup_raw_frame_pop(&raw_frame);
            int16_t uac_frame[WAKEUP_STREAM_SAMPLE_CNT][WAKEUP_UAC_CHANNELS];

            algo_frame = (const int16_t(*)[WAKEUP_STREAM_CHANNELS])((const uint8_t *)data +
                                                                    i * WAKEUP_STREAM_ONE_FRAME_SIZE);
            for (uint32_t s = 0; s < WAKEUP_STREAM_SAMPLE_CNT; s++) {
                int16_t mic = has_raw ? raw_frame.sample[s].mic1 : algo_frame[s][0];
                int16_t mic2 = (has_raw && raw_frame.has_mic2) ? raw_frame.sample[s].mic2 : 0;

                int16_t hard_ref = mic2;
                int16_t soft_ref = (has_raw && raw_frame.has_soft_ref) ? raw_frame.sample[s].soft_ref : algo_frame[s][2];
                int16_t algo_out = algo_frame[s][3];

                uac_frame[s][WAKEUP_UAC_CH_MIC] = mic;
                uac_frame[s][WAKEUP_UAC_CH_HARD_REF] = hard_ref;
                uac_frame[s][WAKEUP_UAC_CH_SOFT_REF] = soft_ref;
                uac_frame[s][WAKEUP_UAC_CH_ALGO_OUT] = algo_out;
            }

            app_usb_audio_write(uac_frame, WAKEUP_STREAM_SAMPLE_CNT, WAKEUP_UAC_CHANNELS, 16);
        }
    }
#endif

#if CONFIG_APP_USB_CDC_ENABLE
    // Send to USB CDC if CDC streaming is enabled
    app_usb_cdc_audio_write(data, len);
#endif
}


static int shell_wakeup_uac_record(int argc, char **argv)
{
    if (argc < 2) {
        return -1;
    }

    if (strncmp(argv[1], "on", 2) == 0) {

        if (wakeup_uac_record_flag == false) {
            wakeup_uac_record_flag = true;
            shellPrint(shellGetCurrent(), "wakeup uac record on success\r\n");
        } else {
            shellPrint(shellGetCurrent(), "wakeup uac record already on\r\n");
        }
    } else if (strncmp(argv[1], "off", 3) == 0) {
        if (wakeup_uac_record_flag) {
            wakeup_uac_record_flag = false;
            shellPrint(shellGetCurrent(), "wakeup uac record off success\r\n");
        } else {
            shellPrint(shellGetCurrent(), "wakeup uac record already off\r\n");
        }
    }

    return 0;
}

#if CONFIG_APP_USB_AUDIO_ENABLE
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 wakeup_uac_record, shell_wakeup_uac_record, "control wakeup UAC recording: wakeup uac record on");
#endif
