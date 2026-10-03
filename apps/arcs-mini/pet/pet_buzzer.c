/*
 * pet_buzzer.c - piezo beep synthesis for the pet
 *
 * The Tamagotchi ROM drives a piezo buzzer; tamalib reports the frequency in
 * dHz and toggles playback without knowing the duration in advance. When the
 * beep stops, pet_hal posts {freq, duration} here. This worker synthesizes a
 * 16 kHz/16-bit/mono square wave into a persistent WAV buffer and plays it
 * through the miniapp player via a mem:// URL (pattern proven by
 * miniapp_runtime.c miniapp_buzzer_task()).
 */
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "alarm_ring.h"
#include "app_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_thread.h"
#include "queue.h"
#include "task.h"
#include "voice_player_comm.h"
#include "voice_player_tts.h"

#include "pet_buzzer.h"

#define TAG "pet"

#ifdef CONFIG_MINIAPP_BUZZER

#define PET_BUZZER_QUEUE_LEN 8
#define PET_BUZZER_MIN_MS 30u
#define PET_BUZZER_MAX_MS 2000u
#define PET_BUZZER_MIN_HZ 100u
#define PET_BUZZER_MAX_HZ 5000u
#define PET_BUZZER_WAV_BYTES (44u + PET_BUZZER_MAX_MS * 32u) /* 16 kHz * 2 B */

typedef struct {
    uint32_t freq_hz;
    uint32_t dur_ms;
} pet_buzzer_cmd_t;

static QueueHandle_t s_queue;
static bool s_dropped_log;

static void pet_buzzer_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static bool pet_buzzer_player_active(void)
{
    app_player_state_t state = app_player_get_state(miniapp_player);
    return state == APP_PLAYER_STATE_PREPARING || state == APP_PLAYER_STATE_PREPARED ||
           state == APP_PLAYER_STATE_PLAYING || state == APP_PLAYER_STATE_PAUSED;
}

static void pet_buzzer_play(const pet_buzzer_cmd_t *cmd)
{
    static uint8_t *wav; /* persistent: the decoder retains the mem:// URL */

    if (!miniapp_player) {
        if (!s_dropped_log) {
            s_dropped_log = true;
            LISA_LOGW(TAG, "buzz: miniapp player unavailable, beeps muted");
        }
        return;
    }
    if (voice_player_tts_is_active() || alarm_ring_is_active()) {
        return; /* voice and alarm always win over the pet */
    }
    if (pet_buzzer_player_active() && app_player_stop(miniapp_player) != APP_PLAYER_OK) {
        return;
    }
    if (!wav) {
        wav = lisa_mem_alloc(PET_BUZZER_WAV_BYTES);
        if (!wav) {
            LISA_LOGW(TAG, "buzz: alloc failed");
            return;
        }
    }

    uint32_t count = cmd->dur_ms * 16u; /* samples @16 kHz */
    uint32_t bytes = count * 2u;
    memset(wav, 0, 44);
    memcpy(wav, "RIFF", 4);
    pet_buzzer_u32(wav + 4, 36u + bytes);
    memcpy(wav + 8, "WAVEfmt ", 8);
    pet_buzzer_u32(wav + 16, 16);
    wav[20] = 1; /* PCM */
    wav[22] = 1; /* mono */
    pet_buzzer_u32(wav + 24, 16000);  /* sample rate */
    pet_buzzer_u32(wav + 28, 32000);  /* byte rate */
    wav[32] = 2; /* block align */
    wav[34] = 16; /* bits */
    memcpy(wav + 36, "data", 4);
    pet_buzzer_u32(wav + 40, bytes);

    uint32_t phase = 0;
    for (uint32_t i = 0; i < count; ++i) {
        phase = (phase + cmd->freq_hz) % 16000u;
        int16_t sample = phase < 8000u ? 12000 : -12000;
        wav[44u + i * 2u] = (uint8_t)sample;
        wav[45u + i * 2u] = (uint8_t)((uint16_t)sample >> 8);
    }

    char url[64];
    snprintf(url, sizeof(url), "mem://addr=%usize=%u", (unsigned)(uintptr_t)wav, bytes + 44u);
    if (app_player_play(miniapp_player, url) != APP_PLAYER_OK) {
        LISA_LOGW(TAG, "buzz: play failed");
        return;
    }

    /* wait for the segment to finish (or be preempted) before the next one */
    TickType_t started = xTaskGetTickCount();
    while (!voice_player_tts_is_active() && !alarm_ring_is_active() &&
           xTaskGetTickCount() - started < pdMS_TO_TICKS(cmd->dur_ms + 500u)) {
        app_player_state_t state = app_player_get_state(miniapp_player);
        if (state != APP_PLAYER_STATE_PREPARING && state != APP_PLAYER_STATE_PREPARED &&
            state != APP_PLAYER_STATE_PLAYING) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (pet_buzzer_player_active()) {
        (void)app_player_stop(miniapp_player);
    }
}

static void pet_buzzer_task(void *arg)
{
    (void)arg;
    pet_buzzer_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        pet_buzzer_play(&cmd);
    }
}

void pet_buzzer_post(uint32_t freq_hz, uint32_t dur_ms)
{
    if (!s_queue) {
        return;
    }
    if (freq_hz < PET_BUZZER_MIN_HZ || freq_hz > PET_BUZZER_MAX_HZ ||
        dur_ms < PET_BUZZER_MIN_MS) {
        return;
    }
    pet_buzzer_cmd_t cmd = {
        .freq_hz = freq_hz,
        .dur_ms = dur_ms > PET_BUZZER_MAX_MS ? PET_BUZZER_MAX_MS : dur_ms,
    };
    (void)xQueueSend(s_queue, &cmd, 0); /* drop when the worker is behind */
}

int pet_buzzer_init(void)
{
    if (s_queue) {
        return 0;
    }
    s_queue = xQueueCreate(PET_BUZZER_QUEUE_LEN, sizeof(pet_buzzer_cmd_t));
    if (!s_queue) {
        return -1;
    }
    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"pet.buzz",
        .stack_size = 2048,
        .priority = LISA_OS_PRIORITY_IDLE, /* below everything, beeps may lag */
    };
    if (!lisa_thread_create(&attr, pet_buzzer_task, NULL)) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return -1;
    }
    return 0;
}

#else /* !CONFIG_MINIAPP_BUZZER: no miniapp player, beeps are muted */

int pet_buzzer_init(void)
{
    return 0;
}

void pet_buzzer_post(uint32_t freq_hz, uint32_t dur_ms)
{
    (void)freq_hz;
    (void)dur_ms;
}

#endif
