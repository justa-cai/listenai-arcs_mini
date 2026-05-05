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

#include "usb_config.h"
#include "usbh_audio.h"
#include "usbh_core.h"
#include "usbh_serial.h"
#include "usbh_video.h"

#include "sys_init.h"
#include "sysheap.h"
#include "video_display.h"

#define VIDEO_FORMAT_PREF              USBH_VIDEO_FORMAT_MJPEG
#define MAX_VIDEO_DEVICES              CONFIG_USBHOST_MAX_VIDEO_CLASS
#define SERIAL_TEST_LEN                (10 * 1024)
#define SERIAL_POLL_INTERVAL_MS        10
#define SERIAL_ROUND_INTERVAL_MS       500
#define SERIAL_PROGRESS_LOG_STEP       1024
#define SERIAL_IDLE_TIMEOUT_MS         5000
#define SINE_TABLE_LEN                 256
#define REC_DUMP_MAX_BYTES             64
#define REC_DUMP_INTERVAL              500
/* BK MUSB host only provides 8 pipes, so keep UAC in speaker-playback-only
 * mode by default to leave enough pipes for concurrent UVC + CDC devices.
 */
#define AUDIO_PLAYBACK_ONLY  1

struct video_dev_ctx {
    struct usbh_video *video_class;
    uint8_t *frame_buf;
    uint8_t *chunk_buf;
    volatile bool running;
    volatile bool disconnected;
    TaskHandle_t task_handle;
    uint32_t frame_count;
    uint32_t total_bytes;
    uint16_t width;
    uint16_t height;
};

struct stream_info {
    bool found;
    char name[16];
    uint32_t samp_freq;
    uint8_t bitresolution;
    uint8_t channels;
    uint8_t subframe_size;
};

struct audio_ctx {
    struct usbh_audio *audio_class;
    volatile bool running;
    volatile bool disconnected;
    TaskHandle_t task_handle;
    struct stream_info speaker;
    struct stream_info mic;
    uint32_t sine_phase;
    uint32_t sine_phase_inc;
    uint32_t rec_total_bytes;
    uint32_t rec_dump_count;
};

static struct video_dev_ctx g_devs[MAX_VIDEO_DEVICES];
static struct audio_ctx g_audio_ctx;

static volatile uint32_t serial_tx_bytes;
static volatile uint32_t serial_rx_bytes;
static volatile bool serial_is_opened;
static volatile bool serial_device_disconnected;

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

static bool find_best_format(struct usbh_video *vc, uint8_t pref_fmt,
                             uint8_t *out_fmt, uint16_t *out_w, uint16_t *out_h)
{
    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].format_type == pref_fmt && vc->format[i].num_of_frames > 0) {
            *out_fmt = pref_fmt;
            *out_w = vc->format[i].frame[0].wWidth;
            *out_h = vc->format[i].frame[0].wHeight;
            return true;
        }
    }

    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].num_of_frames > 0) {
            *out_fmt = vc->format[i].format_type;
            *out_w = vc->format[i].frame[0].wWidth;
            *out_h = vc->format[i].frame[0].wHeight;
            return true;
        }
    }

    return false;
}

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

        for (uint8_t j = 1; j < audio_class->as_msg_table[i].num_of_altsetting; j++) {
            struct audio_cs_if_as_format_type_descriptor *fmt =
                &audio_class->as_msg_table[i].as_format[j];
            uint32_t freq = 0;
            bool found_48k = false;

            if (fmt->bSamFreqType == 0) {
                continue;
            }

            for (uint8_t k = 0; k < fmt->bSamFreqType; k++) {
                memcpy(&freq, &fmt->tSamFreq[3 * k], 3);
                if (freq == 48000) {
                    found_48k = true;
                    break;
                }
            }
            if (!found_48k) {
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

static void on_video_frame(struct usbh_video *video_class,
                           uint8_t *frame_buf,
                           uint32_t frame_size,
                           void *arg)
{
    struct video_dev_ctx *ctx = (struct video_dev_ctx *)arg;

    ctx->frame_count++;
    ctx->total_bytes += frame_size;

    if ((ctx->frame_count % 30) == 0) {
        printf("[VIDEO%u] frame #%lu  size=%lu B  total=%lu KB\r\n",
               (unsigned)video_class->minor,
               (unsigned long)ctx->frame_count,
               (unsigned long)frame_size,
               (unsigned long)(ctx->total_bytes / 1024));
    }

    if (ctx->frame_count == 1 && frame_size >= 4) {
        printf("[VIDEO%u] first frame header: %02X %02X %02X %02X\r\n",
               (unsigned)video_class->minor,
               frame_buf[0], frame_buf[1], frame_buf[2], frame_buf[3]);
    }

    video_display_update(video_class->minor,
                         frame_buf, frame_size,
                         ctx->width, ctx->height,
                         video_class->current_format);
}

static void on_playback_fill(struct usbh_audio *audio_class,
                             uint8_t *buf, uint32_t len, void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    uint8_t ch = ctx->speaker.channels;
    uint8_t bytes_per_sample = ctx->speaker.bitresolution / 8;
    uint8_t frame_size = ch * bytes_per_sample;
    uint32_t num_frames;
    int16_t *out = (int16_t *)buf;

    (void)audio_class;

    if (frame_size == 0 || ch == 0) {
        return;
    }

    num_frames = len / frame_size;
    for (uint32_t i = 0; i < num_frames; i++) {
        uint8_t idx = (ctx->sine_phase >> 16) & 0xFF;
        int16_t sample = sine_table[idx];
        ctx->sine_phase += ctx->sine_phase_inc;

        for (uint8_t c = 0; c < ch; c++) {
            out[i * ch + c] = sample;
        }
    }
}

static void on_audio_data(struct usbh_audio *audio_class,
                          uint8_t *data, uint32_t len, void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    uint32_t dump_len;

    (void)audio_class;

    ctx->rec_total_bytes += len;
    ctx->rec_dump_count++;

    if ((ctx->rec_dump_count % REC_DUMP_INTERVAL) == 1) {
        printf("[REC] total: %lu bytes, this packet: %lu bytes\r\n",
               (unsigned long)ctx->rec_total_bytes,
               (unsigned long)len);

        dump_len = len < REC_DUMP_MAX_BYTES ? len : REC_DUMP_MAX_BYTES;
        printf("[REC] data[0..%lu]: ", (unsigned long)(dump_len - 1));
        for (uint32_t i = 0; i < dump_len; i++) {
            printf("%02X", data[i]);
        }
        printf("\r\n");
    }
}

static void video_stream_task(void *arg)
{
    struct video_dev_ctx *ctx = (struct video_dev_ctx *)arg;
    struct usbh_video *video_class = ctx->video_class;
    int ret;
    uint8_t dev_idx = video_class->minor;
    uint8_t actual_fmt;
    uint16_t actual_w;
    uint16_t actual_h;
    uint32_t frame_bufsize;
    uint32_t chunk_size = 0;
    uint8_t altsetting = 0;

    ctx->task_handle = xTaskGetCurrentTaskHandle();

    if (!find_best_format(video_class, VIDEO_FORMAT_PREF, &actual_fmt, &actual_w, &actual_h)) {
        printf("[VIDEO%u] No valid format/frame in device descriptor\r\n", (unsigned)dev_idx);
        goto out_no_stream;
    }

    printf("[VIDEO%u] Opening device (format=%s %ux%u)...\r\n",
           (unsigned)dev_idx,
           (actual_fmt == USBH_VIDEO_FORMAT_MJPEG) ? "MJPEG" : "UNCOMPRESSED",
           (unsigned)actual_w, (unsigned)actual_h);

    if (!video_class->is_bulk && video_class->num_of_intf_altsettings > 1) {
        uint16_t best_mps = 0;

        for (uint8_t i = 1; i < video_class->num_of_intf_altsettings; i++) {
            struct usb_endpoint_descriptor *ep =
                &video_class->hport->config.intf[video_class->data_intf].altsetting[i].ep[0].ep_desc;
            uint16_t mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize) *
                           (USB_GET_MULT(ep->wMaxPacketSize) + 1);

            if (mps > best_mps) {
                best_mps = mps;
                altsetting = i;
            }
        }

        printf("[VIDEO%u] ISO mode: selected altsetting %u (mps=%u)\r\n",
               (unsigned)dev_idx,
               (unsigned)altsetting,
               (unsigned)best_mps);
    }

    ret = usbh_video_open(video_class, actual_fmt, actual_w, actual_h, altsetting);
    if (ret < 0) {
        printf("[VIDEO%u] Open failed: %d\r\n", (unsigned)dev_idx, ret);
        goto out_no_stream;
    }

    ctx->width = actual_w;
    ctx->height = actual_h;

    frame_bufsize = (uint32_t)actual_w * actual_h * 2;
    ctx->frame_buf = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, frame_bufsize);
    if (!ctx->frame_buf) {
        printf("[VIDEO%u] Failed to alloc frame_buf (%lu B)\r\n",
               (unsigned)dev_idx, (unsigned long)frame_bufsize);
        usbh_video_close(video_class);
        goto out_no_stream;
    }

    printf("[VIDEO%u] frame_buf allocated: %lu B (%ux%u)\r\n",
           (unsigned)dev_idx, (unsigned long)frame_bufsize,
           (unsigned)actual_w, (unsigned)actual_h);

    if (video_class->is_bulk) {
        chunk_size = video_class->probe.dwMaxPayloadTransferSize;
        if (chunk_size == 0) {
            chunk_size = CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE;
        }

        ctx->chunk_buf = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, chunk_size);
        if (!ctx->chunk_buf) {
            printf("[VIDEO%u] Failed to allocate chunk_buf (%lu B)\r\n",
                   (unsigned)dev_idx, (unsigned long)chunk_size);
            usbh_video_close(video_class);
            goto out_no_stream;
        }

        printf("[VIDEO%u] chunk_buf allocated: %lu B\r\n",
               (unsigned)dev_idx, (unsigned long)chunk_size);
    }
    printf("[VIDEO%u] Starting %s streaming...\r\n",
           (unsigned)dev_idx, video_class->is_bulk ? "bulk" : "ISO");

    ctx->frame_count = 0;
    ctx->total_bytes = 0;

    ret = usbh_video_start_streaming(video_class,
                                     ctx->frame_buf,
                                     frame_bufsize,
                                     ctx->chunk_buf,
                                     chunk_size,
                                     on_video_frame,
                                     ctx);
    if (ret < 0) {
        printf("[VIDEO%u] start_streaming failed: %d\r\n", (unsigned)dev_idx, ret);
        if (ctx->chunk_buf) {
            psram_free(ctx->chunk_buf);
            ctx->chunk_buf = NULL;
        }
        usbh_video_close(video_class);
        goto out_no_stream;
    }

    printf("[VIDEO%u] Streaming started\r\n", (unsigned)dev_idx);

    while (!ctx->disconnected) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (ctx->disconnected) {
            break;
        }
        if (!video_class->streaming) {
            printf("[VIDEO%u] Streaming stopped (device error), waiting for disconnect...\r\n",
                   (unsigned)dev_idx);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            break;
        }
        printf("[VIDEO%u] Running: %lu frames, %lu KB total\r\n",
               (unsigned)dev_idx,
               (unsigned long)ctx->frame_count,
               (unsigned long)(ctx->total_bytes / 1024));
    }

    printf("[VIDEO%u] Device disconnected, cleaning up...\r\n", (unsigned)dev_idx);
    if (ctx->chunk_buf) {
        psram_free(ctx->chunk_buf);
        ctx->chunk_buf = NULL;
    }
    goto out;

out_no_stream:
    ;

out:
    psram_free(ctx->frame_buf);
    ctx->frame_buf = NULL;
    ctx->task_handle = NULL;
    ctx->video_class = NULL;
    ctx->running = false;
    vTaskDelete(NULL);
}

static void audio_task(void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    struct usbh_audio *audio_class = ctx->audio_class;
    int ret;

    ctx->task_handle = xTaskGetCurrentTaskHandle();

    find_stream_format(audio_class, "speaker", "headphones", &ctx->speaker);
    find_stream_format(audio_class, "mic", NULL, &ctx->mic);

#if AUDIO_PLAYBACK_ONLY
    if (ctx->mic.found) {
        printf("[AUDIO] Mic stream detected but disabled by default (playback only mode)\r\n");
        ctx->mic.found = false;
    }
#endif

#if AUDIO_PLAYBACK_ONLY
    if (!ctx->speaker.found) {
        printf("[AUDIO] No speaker stream found (playback only mode)\r\n");
        goto out;
    }
#else
    if (!ctx->speaker.found && !ctx->mic.found) {
        printf("[AUDIO] No speaker or mic stream found\r\n");
        goto out;
    }
#endif

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
            ret = usbh_audio_set_mute(audio_class, ctx->speaker.name, 0, false);
            printf("[AUDIO] speaker unmute: %d\r\n", ret);
            ret = usbh_audio_set_volume(audio_class, ctx->speaker.name, 0, 0);
            printf("[AUDIO] speaker volume 0dB: %d\r\n", ret);
        }
    }

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

#if AUDIO_PLAYBACK_ONLY
    if (!ctx->speaker.found) {
        printf("[AUDIO] Speaker stream failed to open in playback only mode\r\n");
        goto out;
    }
#else
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
#endif

    if (!ctx->speaker.found && !ctx->mic.found) {
        printf("[AUDIO] All streams failed to open\r\n");
        goto out;
    }

    printf("[AUDIO] Active:%s%s\r\n",
           ctx->speaker.found ? " playback" : "",
           ctx->mic.found ? " recording" : "");

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

static void usbh_serial_thread(void *argument)
{
    int ret;
    const char *serial_devname = "/dev/ttyACM0";
    struct usbh_serial *serial;
    uint8_t *serial_tx_buffer = NULL;
    uint8_t *serial_rx_data = NULL;
    bool serial_test_success = false;
    uint32_t serial_test_round = 0;

    (void)argument;

    serial = usbh_serial_open(serial_devname, USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
    if (serial == NULL) {
        serial_devname = "/dev/ttyUSB0";
        serial = usbh_serial_open(serial_devname, USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);
        if (serial == NULL) {
            printf("[SERIAL] No supported serial device found (/dev/ttyACM0 or /dev/ttyUSB0)\r\n");
            goto delete;
        }
    }

    serial_tx_buffer = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, SERIAL_TEST_LEN);
    serial_rx_data = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, SERIAL_TEST_LEN);
    if (serial_tx_buffer == NULL || serial_rx_data == NULL) {
        printf("[SERIAL] Failed to allocate serial test buffers in PSRAM\r\n");
        goto delete_with_close;
    }

    printf("[SERIAL] Serial device opened successfully: %s\r\n", serial_devname);

    struct usbh_serial_termios termios;
    memset(&termios, 0, sizeof(termios));
    termios.baudrate = 115200;
    termios.stopbits = 0;
    termios.parity = 0;
    termios.databits = 8;
    termios.rtscts = false;
    termios.rx_timeout = 0;

    printf("[SERIAL] Configuring serial port: 115200 8N1\r\n");
    ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
    if (ret < 0) {
        printf("[SERIAL] Set serial attr error, ret:%d\r\n", ret);
        goto delete_with_close;
    }

    printf("[SERIAL] Serial port configured, waiting for device ready...\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    printf("[SERIAL] Generating test pattern (incremental sequence 0x00-0xFF)...\r\n");
    for (uint32_t i = 0; i < SERIAL_TEST_LEN; i++) {
        serial_tx_buffer[i] = i & 0xFF;
    }


    while (!serial_device_disconnected) {
        uint32_t error_count = 0;
        int timeout = SERIAL_IDLE_TIMEOUT_MS / SERIAL_POLL_INTERVAL_MS;
        uint32_t next_tx_log = SERIAL_PROGRESS_LOG_STEP;
        uint32_t next_rx_log = SERIAL_PROGRESS_LOG_STEP;
        bool serial_round_aborted = false;

        serial_test_round++;
        serial_test_success = false;
        serial_tx_bytes = 0;
        serial_rx_bytes = 0;
        memset(serial_rx_data, 0, SERIAL_TEST_LEN);

        printf("[SERIAL] Start serial loopback test #%lu, len: %d\r\n",
               (unsigned long)serial_test_round, SERIAL_TEST_LEN);

        while (1) {
            if (serial_device_disconnected) {
                printf("[SERIAL] Device disconnected during transmission\r\n");
                goto delete_with_close;
            }

            ret = usbh_serial_write(serial, serial_tx_buffer + serial_tx_bytes,
                                    SERIAL_TEST_LEN - serial_tx_bytes);
            if (ret < 0) {
                printf("[SERIAL] Serial write error, ret:%d (sent %lu bytes)\r\n",
                       ret, (unsigned long)serial_tx_bytes);
                printf("[SERIAL] Restarting serial RX/TX path after transfer error...\r\n");
                ret = usbh_serial_control(serial, USBH_SERIAL_CMD_SET_ATTR, &termios);
                if (ret < 0) {
                    printf("[SERIAL] Serial recovery failed, ret:%d\r\n", ret);
                    goto delete_with_close;
                }
                printf("[SERIAL] Treating this round as failed and waiting for the next loopback round\r\n");
                serial_round_aborted = true;
                break;
            } else if (ret == 0) {
                vTaskDelay(pdMS_TO_TICKS(SERIAL_POLL_INTERVAL_MS));
                continue;
            } else {
                serial_tx_bytes += ret;
                if (serial_tx_bytes >= next_tx_log || serial_tx_bytes >= SERIAL_TEST_LEN) {
                    printf("[SERIAL][TX] Progress: %lu/%d\r\n",
                           (unsigned long)serial_tx_bytes, SERIAL_TEST_LEN);
                    while (next_tx_log <= serial_tx_bytes) {
                        next_tx_log += SERIAL_PROGRESS_LOG_STEP;
                    }
                }

                if (serial_tx_bytes >= SERIAL_TEST_LEN) {
                    printf("[SERIAL] Send over\r\n");
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(SERIAL_POLL_INTERVAL_MS));
        }

        if (!serial_round_aborted) {
            printf("[SERIAL] Waiting for loopback data...\r\n");
            while (1) {
                if (serial_device_disconnected) {
                    printf("[SERIAL] Device disconnected during reception\r\n");
                    goto delete_with_close;
                }

                ret = usbh_serial_read(serial, serial_rx_data + serial_rx_bytes,
                                       SERIAL_TEST_LEN - serial_rx_bytes);
                if (ret > 0) {
                    serial_rx_bytes += ret;
                    if (serial_rx_bytes >= next_rx_log || serial_rx_bytes >= SERIAL_TEST_LEN) {
                        printf("[SERIAL][RX] Progress: %lu/%d\r\n",
                               (unsigned long)serial_rx_bytes, SERIAL_TEST_LEN);
                        while (next_rx_log <= serial_rx_bytes) {
                            next_rx_log += SERIAL_PROGRESS_LOG_STEP;
                        }
                    }
                    if (serial_rx_bytes >= SERIAL_TEST_LEN) {
                        printf("[SERIAL] Receive over\r\n");
                        break;
                    }
                    timeout = SERIAL_IDLE_TIMEOUT_MS / SERIAL_POLL_INTERVAL_MS;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(SERIAL_POLL_INTERVAL_MS));
                    timeout--;
                    if (timeout == 0) {
                        printf("[SERIAL] Serial read timeout after %d ms idle (received %lu/%d bytes)\r\n",
                               SERIAL_IDLE_TIMEOUT_MS,
                               (unsigned long)serial_rx_bytes,
                               SERIAL_TEST_LEN);
                        if (serial_rx_bytes == 0) {
                            printf("[SERIAL] No loopback data received, please short TX/RX\r\n");
                        }
                        break;
                    }
                }
            }
        }

        if (serial_rx_bytes >= SERIAL_TEST_LEN) {
            printf("[SERIAL] Verifying loopback data...\r\n");
            for (uint32_t i = 0; i < SERIAL_TEST_LEN; i++) {
                uint8_t expected = i & 0xFF;
                if (serial_rx_data[i] != expected) {
                    if (error_count < 10) {
                        printf("[SERIAL] Data mismatch at index %lu: expected 0x%02x, got 0x%02x\r\n",
                               (unsigned long)i, expected, serial_rx_data[i]);
                    }
                    error_count++;
                }
            }

            if (error_count > 0) {
                printf("[SERIAL] Total %lu bytes mismatched out of %d\r\n",
                       (unsigned long)error_count, SERIAL_TEST_LEN);
            } else {
                serial_test_success = true;
                printf("[SERIAL] All %d bytes verified correctly!\r\n", SERIAL_TEST_LEN);
            }
        }

        printf("\r\n========================================\r\n");
        printf("  Serial Loopback Test #%lu: %s\r\n",
               (unsigned long)serial_test_round,
               serial_test_success ? "SUCCESS" : "FAILED");
        printf("========================================\r\n");

        if (serial_device_disconnected) {
            break;
        }

        printf("[SERIAL] Next loopback round starts in 500 ms...\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

delete_with_close:
    printf("[SERIAL] Closing serial device...\r\n");

    if (serial_tx_buffer != NULL) {
        psram_free(serial_tx_buffer);
    }
    if (serial_rx_data != NULL) {
        psram_free(serial_rx_data);
    }

    if (serial != NULL) {
        usbh_serial_close(serial);
        serial = NULL;
    }

delete:
    while (!serial_device_disconnected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("[SERIAL] Device disconnected, cleaning up...\r\n");
    serial_is_opened = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("[SERIAL] Serial thread exiting safely\r\n");
    vTaskDelete(NULL);
}

void usbh_video_run(struct usbh_video *video_class)
{
    uint8_t idx = video_class->minor;
    struct video_dev_ctx *ctx;
    char task_name[16];

    if (idx >= MAX_VIDEO_DEVICES) {
        printf("[VIDEO] minor=%u exceeds MAX_VIDEO_DEVICES=%d, ignored\r\n",
               (unsigned)idx, MAX_VIDEO_DEVICES);
        return;
    }

    ctx = &g_devs[idx];
    if (ctx->running) {
        printf("[VIDEO%u] Already running, ignoring hot-plug\r\n", (unsigned)idx);
        return;
    }

    printf("[VIDEO%u] Camera connected: %s\r\n",
           (unsigned)idx,
           video_class->hport->config.intf[video_class->ctrl_intf].devname);
    usbh_video_list_info(video_class);

    ctx->video_class = video_class;
    ctx->frame_buf = NULL;
    ctx->chunk_buf = NULL;
    ctx->task_handle = NULL;
    ctx->running = true;
    ctx->disconnected = false;
    ctx->frame_count = 0;
    ctx->total_bytes = 0;

    snprintf(task_name, sizeof(task_name), "video%u", (unsigned)idx);
    xTaskCreate(video_stream_task, task_name, 4096, ctx,
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

void usbh_video_stop(struct usbh_video *video_class)
{
    uint8_t idx = video_class->minor;
    struct video_dev_ctx *ctx;

    if (idx >= MAX_VIDEO_DEVICES) {
        return;
    }

    printf("[VIDEO%u] Camera disconnected, stopping streaming\r\n", (unsigned)idx);
    usbh_video_stop_streaming(video_class);

    ctx = &g_devs[idx];
    ctx->disconnected = true;
    if (ctx->task_handle) {
        xTaskNotifyGive(ctx->task_handle);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}

void usbh_serial_run(struct usbh_serial *serial)
{
    (void)serial;

    if (serial_is_opened) {
        printf("[SERIAL] Serial thread already running, ignoring...\r\n");
        return;
    }

    serial_is_opened = true;
    serial_device_disconnected = false;
    printf("[SERIAL] Creating serial thread...\r\n");
    xTaskCreate(usbh_serial_thread, "usbh_serial", 2048, NULL,
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

void usbh_serial_stop(struct usbh_serial *serial)
{
    (void)serial;

    printf("[SERIAL] Stopping serial thread (device disconnected)...\r\n");
    serial_device_disconnected = true;
    serial_is_opened = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("[SERIAL] Serial thread stopped\r\n");
}

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

static void usbh_event_handler(uint8_t busid, uint8_t hub_index, uint8_t hub_port,
                               uint8_t intf, uint8_t event)
{
    const char *event_str[] = {
        "UNKNOWN",
        "CONNECTED",
        "DISCONNECTED",
        "REMOVED"
    };

    printf("\r\n[USB Event] busid=%d, hub_index=%d, hub_port=%d, intf=%d, event=%d (%s)\r\n",
           busid, hub_index, hub_port, intf, event,
           (event < 4) ? event_str[event] : "INVALID");
}

int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  CherryUSB Host Video + Serial + Audio Example\r\n");
    printf("  Video format pref: %s\r\n",
           (VIDEO_FORMAT_PREF == USBH_VIDEO_FORMAT_MJPEG) ? "MJPEG" : "UNCOMPRESSED");
    printf("  Audio mode: playback only (default)\r\n");
    printf("  Max cameras: %d\r\n", MAX_VIDEO_DEVICES);
    printf("========================================\r\n\r\n");

    printf("[INFO] Initializing display...\r\n");
    video_display_init();

    printf("[INFO] Initializing USB Host...\r\n");
    usbh_initialize(0, 0x41000000UL, usbh_event_handler);
    printf("[INFO] USB Host initialized\r\n");
    printf("[INFO] Please connect USB cameras, CDC ACM, and USB audio devices\r\n\r\n");

    {
        int count = 0;
        while (1) {
            int active_video = 0;
            vTaskDelay(pdMS_TO_TICKS(5000));
            count++;

            for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
                if (g_devs[i].running) {
                    active_video++;
                }
            }

            if (active_video == 0 && !serial_is_opened && !g_audio_ctx.running) {
                printf("[%d] Waiting for USB camera, CDC ACM, or USB audio device...\r\n", count);
            }
        }
    }

    return 0;
}

static int usb_host_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;

    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
