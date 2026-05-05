/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "ClockManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usbh_core.h"
#include "usbh_audio.h"

#include "sys_init.h"
#include "usb_config.h"

/* ------------------------------------------------------------------ */
/*  1 kHz sine wave lookup table (one full period, 256 entries)        */
/*  Amplitude ~80% of full scale to avoid clipping                     */
/*  Use fixed-point phase accumulator for any sample rate              */
/* ------------------------------------------------------------------ */
#define SINE_TABLE_LEN 256

static const int16_t sine_table[SINE_TABLE_LEN] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
    32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
    27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
    18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
     6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
        0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
    -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
   -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
   -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
   -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
   -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
   -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
   -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
   -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
   -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
   -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
   -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
   -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
   -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
   -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
    -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804,
};

/* ------------------------------------------------------------------ */
/*  Audio streaming context                                            */
/* ------------------------------------------------------------------ */
struct stream_info {
    bool found;
    char name[16];       /* "speaker" / "headphones" / "mic" */
    uint32_t samp_freq;
    uint8_t bitresolution;
    uint8_t channels;
    uint8_t subframe_size; /* bytes per sample per channel */
};

struct audio_ctx {
    struct usbh_audio *audio_class;
    volatile bool running;
    volatile bool disconnected;
    TaskHandle_t task_handle;

    struct stream_info speaker;
    struct stream_info mic;

    /* Playback state */
    uint32_t sine_phase;     /* 16.16 fixed-point phase (index into 256-entry table) */
    uint32_t sine_phase_inc; /* phase increment per sample for 1kHz */

    /* Recording state */
    uint32_t rec_total_bytes;
    uint32_t rec_dump_count;
};

static struct audio_ctx g_audio_ctx;

/* Max bytes to hex-dump per callback (avoid flooding serial) */
#define REC_DUMP_MAX_BYTES 64
#define REC_DUMP_INTERVAL  500  /* dump every N callbacks */

/* ------------------------------------------------------------------ */
/*  Find a stream by name pattern, return best format                  */
/* ------------------------------------------------------------------ */
static bool find_stream_format(struct usbh_audio *audio_class,
                               const char *name1, const char *name2,
                               struct stream_info *info)
{
    for (uint8_t i = 0; i < audio_class->stream_intf_num; i++) {
        const char *sname = audio_class->as_msg_table[i].stream_name;
        if (!sname) {
            continue;
        }
        if (strcmp(sname, name1) != 0 && (!name2 || strcmp(sname, name2) != 0)) {
            continue;
        }

        /* Search altsettings for best format (prefer 48kHz, then any) */
        for (uint8_t j = 1; j < audio_class->as_msg_table[i].num_of_altsetting; j++) {
            struct audio_cs_if_as_format_type_descriptor *fmt =
                &audio_class->as_msg_table[i].as_format[j];

            if (fmt->bSamFreqType == 0) {
                continue;
            }

            /* Try to find 48kHz first */
            uint32_t freq = 0;
            bool found_48k = false;
            for (uint8_t k = 0; k < fmt->bSamFreqType; k++) {
                memcpy(&freq, &fmt->tSamFreq[3 * k], 3);
                if (freq == 48000) {
                    found_48k = true;
                    break;
                }
            }
            if (!found_48k) {
                /* Fall back to first supported rate */
                memcpy(&freq, &fmt->tSamFreq[0], 3);
            }

            info->found = true;
            strncpy(info->name, sname, sizeof(info->name) - 1);
            info->name[sizeof(info->name) - 1] = '\0';
            info->samp_freq = freq;
            info->bitresolution = fmt->bBitResolution;
            info->channels = fmt->bNrChannels;
            info->subframe_size = fmt->bSubframeSize;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  Playback callback: fill buffer with 1kHz sine wave                 */
/*  Adapts to any channel count and sample rate                        */
/* ------------------------------------------------------------------ */
static void on_playback_fill(struct usbh_audio *audio_class,
                             uint8_t *buf, uint32_t len, void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    uint8_t ch = ctx->speaker.channels;
    uint8_t bytes_per_sample = ctx->speaker.bitresolution / 8;
    uint8_t frame_size = ch * bytes_per_sample; /* bytes per frame */

    if (frame_size == 0 || ch == 0) {
        return;
    }

    int16_t *out = (int16_t *)buf;
    uint32_t num_frames = len / frame_size;

    for (uint32_t i = 0; i < num_frames; i++) {
        /* Look up sine value using upper 8 bits of 16.16 phase */
        uint8_t idx = (ctx->sine_phase >> 16) & 0xFF;
        int16_t sample = sine_table[idx];
        ctx->sine_phase += ctx->sine_phase_inc;

        /* Fill all channels with the same sample */
        for (uint8_t c = 0; c < ch; c++) {
            out[i * ch + c] = sample;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Recording callback: receive captured audio data                    */
/* ------------------------------------------------------------------ */
static void on_audio_data(struct usbh_audio *audio_class,
                          uint8_t *data, uint32_t len, void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;

    ctx->rec_total_bytes += len;
    ctx->rec_dump_count++;

    /* Periodic status + hex dump */
    if ((ctx->rec_dump_count % REC_DUMP_INTERVAL) == 1) {
        printf("[REC] total: %lu bytes, this packet: %lu bytes\r\n",
               (unsigned long)ctx->rec_total_bytes, (unsigned long)len);

        /* Hex dump first N bytes */
        uint32_t dump_len = len < REC_DUMP_MAX_BYTES ? len : REC_DUMP_MAX_BYTES;
        printf("[REC] data[0..%lu]: ", (unsigned long)dump_len - 1);
        for (uint32_t i = 0; i < dump_len; i++) {
            printf("%02X", data[i]);
        }
        printf("\r\n");
    }
}

/* ------------------------------------------------------------------ */
/*  Audio task: auto-detect and start speaker/mic/both                 */
/* ------------------------------------------------------------------ */
static void audio_task(void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    struct usbh_audio *audio_class = ctx->audio_class;
    int ret;

    ctx->task_handle = xTaskGetCurrentTaskHandle();

    /* ---- Detect available streams ---- */
    find_stream_format(audio_class, "speaker", "headphones", &ctx->speaker);
    find_stream_format(audio_class, "mic", NULL, &ctx->mic);

    if (!ctx->speaker.found && !ctx->mic.found) {
        printf("[AUDIO] No speaker or mic stream found\r\n");
        goto out;
    }

    /* ---- Phase 1: Open all streams (SET_INTERFACE) before starting any ---- */
    if (ctx->speaker.found) {
        printf("[AUDIO] Speaker: %s %uch/%ubit/%luHz\r\n",
               ctx->speaker.name,
               (unsigned)ctx->speaker.channels,
               (unsigned)ctx->speaker.bitresolution,
               (unsigned long)ctx->speaker.samp_freq);

        ret = usbh_audio_open(audio_class, ctx->speaker.name,
                              ctx->speaker.samp_freq, ctx->speaker.bitresolution);
        if (ret < 0) {
            printf("[AUDIO] speaker open failed: %d\r\n", ret);
            ctx->speaker.found = false;
        } else {
            /* Unmute and set 0 dB — device may default to muted/minimum volume */
            ret = usbh_audio_set_mute(audio_class, ctx->speaker.name, 0, false);
            printf("[AUDIO] speaker unmute: %d\r\n", ret);
            ret = usbh_audio_set_volume(audio_class, ctx->speaker.name, 0, 0);
            printf("[AUDIO] speaker volume 0dB: %d\r\n", ret);
        }
    }

    if (ctx->mic.found) {
        printf("[AUDIO] Mic: %s %uch/%ubit/%luHz\r\n",
               ctx->mic.name,
               (unsigned)ctx->mic.channels,
               (unsigned)ctx->mic.bitresolution,
               (unsigned long)ctx->mic.samp_freq);

        ret = usbh_audio_open(audio_class, ctx->mic.name,
                              ctx->mic.samp_freq, ctx->mic.bitresolution);
        if (ret < 0) {
            printf("[AUDIO] mic open failed: %d\r\n", ret);
            ctx->mic.found = false;
        }
    }

    /* ---- Phase 2: Start ISO streaming after all interfaces are configured ---- */
    if (ctx->speaker.found) {
        ctx->sine_phase = 0;
        ctx->sine_phase_inc = (uint32_t)(16777216000ULL / ctx->speaker.samp_freq);

        printf("[AUDIO] Starting 1kHz sine wave playback...\r\n");
        ret = usbh_audio_start_playback(audio_class, on_playback_fill, ctx);
        if (ret < 0) {
            printf("[AUDIO] start_playback failed: %d\r\n", ret);
            ctx->speaker.found = false;
        }
    }

    if (ctx->mic.found) {
        ctx->rec_total_bytes = 0;
        ctx->rec_dump_count = 0;

        printf("[AUDIO] Starting recording...\r\n");
        ret = usbh_audio_start_streaming(audio_class, on_audio_data, ctx);
        if (ret < 0) {
            printf("[AUDIO] start_streaming failed: %d\r\n", ret);
            ctx->mic.found = false;
        }
    }

    if (!ctx->speaker.found && !ctx->mic.found) {
        printf("[AUDIO] All streams failed to open\r\n");
        goto out;
    }

    printf("[AUDIO] Active:%s%s\r\n",
           ctx->speaker.found ? " playback" : "",
           ctx->mic.found ? " recording" : "");

    /* Wait for disconnect */
    while (!ctx->disconnected) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));
    }

    printf("[AUDIO] Disconnected, cleaning up\r\n");

out:
    ctx->task_handle = NULL;
    ctx->audio_class = NULL;
    ctx->running = false;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/*  CherryUSB weak overrides                                           */
/* ------------------------------------------------------------------ */
void usbh_audio_run(struct usbh_audio *audio_class)
{
    struct audio_ctx *ctx = &g_audio_ctx;

    if (ctx->running) {
        return;
    }

    printf("[AUDIO] Audio device connected\r\n");
    usbh_audio_list_module(audio_class);

    memset(ctx, 0, sizeof(*ctx));
    ctx->audio_class = audio_class;
    ctx->running = true;
    ctx->disconnected = false;

    xTaskCreate(audio_task, "audio", 4096, ctx,
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

void usbh_audio_stop(struct usbh_audio *audio_class)
{
    struct audio_ctx *ctx = &g_audio_ctx;

    printf("[AUDIO] Disconnected, stopping\r\n");
    usbh_audio_stop_streaming(audio_class);

    ctx->disconnected = true;
    if (ctx->task_handle) {
        xTaskNotifyGive(ctx->task_handle);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ------------------------------------------------------------------ */
/*  USB event callback                                                 */
/* ------------------------------------------------------------------ */
static void usbh_event_handler(uint8_t busid, uint8_t hub_index, uint8_t hub_port,
                                uint8_t intf, uint8_t event)
{
    (void)busid;
    (void)hub_index;
    (void)hub_port;
    (void)intf;
    (void)event;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  CherryUSB Host Audio Example\r\n");
    printf("  Auto: playback / recording / both\r\n");
    printf("========================================\r\n\r\n");

    usbh_initialize(0, 0x41000000UL, usbh_event_handler);
    printf("[INFO] USB Host initialized, connect USB audio device\r\n\r\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!g_audio_ctx.running) {
            printf("[INFO] Waiting for USB audio device...\r\n");
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  USB PHY hardware init (Host mode)                                  */
/* ------------------------------------------------------------------ */
static int usb_host_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
