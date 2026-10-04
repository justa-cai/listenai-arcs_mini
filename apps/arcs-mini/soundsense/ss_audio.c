/*
 * ss_audio.c - microphone PCM tap for SoundSense streaming
 *
 * The audio0 device runs 16 kHz / 16-bit / stereo-interleaved (left = mic,
 * right = AEC reference, see app_wakeup.c). We register a second observer
 * (slot 2 of 4 - the wakeup engine holds slot 1) and copy only the left
 * channel into a drop-oldest ring buffer in PSRAM. The observer callback
 * runs on the audio dispatch thread shared with the wakeup feed, so it
 * only does a deinterleave memcpy and never blocks.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "lisa_audio.h"
#include "lisa_device.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "semphr.h"

#include "ss_audio.h"

#define TAG "ss"

#define SS_RING_BYTES (64u * 1024u) /* ~2 s of mono 16 kHz s16 */

static uint8_t *s_ring;
static uint32_t s_head; /* writer position */
static uint32_t s_tail; /* reader position */
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_data_sem; /* signaled on new data */
static lisa_device_t *s_audio_dev;
static bool s_running;

static void ss_audio_cb(const lisa_audio_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event->record_buffer || event->record_samples == 0 || !s_ring) {
        return;
    }

    /* stereo interleaved s16: [L0 R0 L1 R1 ...], take L (the microphone) */
    const int16_t *stereo = (const int16_t *)event->record_buffer;
    uint32_t frames = event->record_samples / 2u; /* interleaved pairs */

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (uint32_t i = 0; i < frames; i++) {
        s_ring[s_head] = (uint8_t)(stereo[2 * i] & 0xFF);
        s_ring[(s_head + 1) % SS_RING_BYTES] = (uint8_t)((stereo[2 * i] >> 8) & 0xFF);
        s_head = (s_head + 2) % SS_RING_BYTES;
        if (s_head == s_tail) {
            /* drop the oldest sample pair to keep the latest audio */
            s_tail = (s_tail + 2) % SS_RING_BYTES;
        }
    }
    xSemaphoreGive(s_lock);
    xSemaphoreGive(s_data_sem);
}

int ss_audio_start(void)
{
    if (s_running) {
        return 0;
    }

    s_audio_dev = lisa_device_get("audio0");
    if (!s_audio_dev) {
        LISA_LOGE(TAG, "audio: device audio0 not found");
        return -1;
    }

    if (!s_ring) {
        s_ring = lisa_mem_alloc(SS_RING_BYTES);
        s_lock = xSemaphoreCreateMutex();
        s_data_sem = xSemaphoreCreateBinary();
        if (!s_ring || !s_lock || !s_data_sem) {
            LISA_LOGE(TAG, "audio: alloc failed");
            return -1;
        }
    }
    s_head = s_tail = 0;

    if (lisa_audio_register_callback(s_audio_dev, ss_audio_cb, NULL) != 0) {
        LISA_LOGE(TAG, "audio: observer register failed");
        return -1;
    }

    s_running = true;
    LISA_LOGI(TAG, "audio: tap started (mono 16k s16, ring %u B)", SS_RING_BYTES);
    return 0;
}

void ss_audio_stop(void)
{
    if (!s_running) {
        return;
    }
    lisa_audio_unregister_callback(s_audio_dev, ss_audio_cb);
    s_running = false;
    LISA_LOGI(TAG, "audio: tap stopped");
}

bool ss_audio_is_running(void)
{
    return s_running;
}

uint32_t ss_audio_read(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    if (!s_ring) {
        return 0;
    }

    /* wait for at least some data (coarse: any signal means new frames) */
    if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return 0;
    }

    uint32_t copied = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    while (copied + 2 <= len && s_tail != s_head) {
        buf[copied++] = s_ring[s_tail];
        buf[copied++] = s_ring[(s_tail + 1) % SS_RING_BYTES];
        s_tail = (s_tail + 2) % SS_RING_BYTES;
    }
    xSemaphoreGive(s_lock);
    return copied;
}
