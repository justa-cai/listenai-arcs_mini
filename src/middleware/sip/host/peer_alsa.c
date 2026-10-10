/*
 * peer_alsa.c - ALSA 全双工实现（Ubuntu 对端）
 *
 * capture 线程以 20ms（320 帧）为节拍读麦克风；playback 线程以 20ms 超时
 * 收 RTP，超时写静音维持节拍。设备名 default，格式 S16_LE 单声道 8k。
 */
#define _TAG "peer_alsa"

#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sip_g711.h"
#include "sip_rtp.h"
#include "sip_osal.h"

#include "peer_alsa.h"

#define FRAMES_PER_PERIOD 160   /* 8kHz 下 20ms（G.711 一样本一字节） */

static snd_pcm_t *s_cap;
static snd_pcm_t *s_play;
static pthread_t s_cap_thread;
static pthread_t s_play_thread;
static sip_rtp_t s_rtp;
static int s_pt;
static volatile int s_running;
static volatile uint32_t s_tx, s_rx;

static int alsa_open(snd_pcm_t **pcm, int stream)
{
    const char *dev = getenv("SIP_ALSA_DEV");
    if (!dev || !dev[0]) {
        dev = "default";
    }
    int err = snd_pcm_open(pcm, dev, (snd_pcm_stream_t)stream, 0);
    if (err < 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "open %s(%s) failed: %s",
                     stream == SND_PCM_STREAM_CAPTURE ? "capture" : "playback", dev,
                     snd_strerror(err));
        return -1;
    }
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(*pcm, hw);
    snd_pcm_hw_params_set_access(*pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(*pcm, hw, SND_PCM_FORMAT_S16_LE);
    unsigned rate = 8000;
    snd_pcm_hw_params_set_rate_near(*pcm, hw, &rate, NULL);
    unsigned ch = 1;
    snd_pcm_hw_params_set_channels(*pcm, hw, ch);
    snd_pcm_uframes_t frames = FRAMES_PER_PERIOD;
    snd_pcm_hw_params_set_period_size_near(*pcm, hw, &frames, NULL);
    err = snd_pcm_hw_params(*pcm, hw);
    if (err < 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "hw_params failed: %s", snd_strerror(err));
        snd_pcm_close(*pcm);
        *pcm = NULL;
        return -1;
    }
    if (rate != 8000) {
        sip_osal_log(SIP_LOG_WARN, _TAG, "sample rate negotiated to %u", rate);
    }
    snd_pcm_prepare(*pcm);
    return 0;
}

static void *capture_thread(void *arg)
{
    (void)arg;
    int16_t pcm[FRAMES_PER_PERIOD];
    uint8_t enc[FRAMES_PER_PERIOD];
    while (s_running) {
        snd_pcm_sframes_t n = snd_pcm_readi(s_cap, pcm, FRAMES_PER_PERIOD);
        if (n == -EPIPE) {
            snd_pcm_prepare(s_cap);
            continue;
        }
        if (n <= 0) {
            usleep(5000);
            continue;
        }
        sip_g711_encode(s_pt, pcm, enc, (uint32_t)n);
        if (sip_rtp_send_audio(&s_rtp, enc, (int)n) > 0) {
            s_tx++;
        }
    }
    return NULL;
}

static void *playback_thread(void *arg)
{
    (void)arg;
    int16_t pcm[FRAMES_PER_PERIOD];
    uint8_t net[FRAMES_PER_PERIOD];
    while (s_running) {
        int n = sip_rtp_recv_audio(&s_rtp, net, sizeof(net), 20);
        if (n == (int)FRAMES_PER_PERIOD) {
            sip_g711_decode(s_pt, net, pcm, FRAMES_PER_PERIOD);
            s_rx++;
        } else {
            memset(pcm, 0, sizeof(pcm)); /* 缺包补静音，维持播放节拍 */
        }
        snd_pcm_sframes_t w = snd_pcm_writei(s_play, pcm, FRAMES_PER_PERIOD);
        if (w == -EPIPE) {
            snd_pcm_prepare(s_play);
        }
    }
    return NULL;
}

int peer_alsa_start(int audio_pt, const char *remote_ip, uint16_t remote_port,
                    uint16_t rtp_local_port)
{
    if (s_running) {
        return 0;
    }
    s_pt = audio_pt;
    if (alsa_open(&s_cap, SND_PCM_STREAM_CAPTURE) != 0 ||
        alsa_open(&s_play, SND_PCM_STREAM_PLAYBACK) != 0) {
        goto fail;
    }
    if (sip_rtp_open(&s_rtp, rtp_local_port, (uint8_t)audio_pt) != 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "rtp bind %u failed", rtp_local_port);
        goto fail;
    }
    sip_rtp_set_peer(&s_rtp, remote_ip, remote_port);
    s_tx = s_rx = 0;
    s_running = 1;
    pthread_create(&s_cap_thread, NULL, capture_thread, NULL);
    pthread_create(&s_play_thread, NULL, playback_thread, NULL);
    sip_osal_log(SIP_LOG_INFO, _TAG, "alsa duplex to %s:%u pt=%d",
                 remote_ip, remote_port, audio_pt);
    return 0;
fail:
    if (s_cap) {
        snd_pcm_close(s_cap);
        s_cap = NULL;
    }
    if (s_play) {
        snd_pcm_close(s_play);
        s_play = NULL;
    }
    return -1;
}

void peer_alsa_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = 0;
    sip_rtp_close(&s_rtp);
    pthread_join(s_cap_thread, NULL);
    pthread_join(s_play_thread, NULL);
    if (s_cap) {
        snd_pcm_close(s_cap);
        s_cap = NULL;
    }
    if (s_play) {
        snd_pcm_close(s_play);
        s_play = NULL;
    }
    sip_osal_log(SIP_LOG_INFO, _TAG, "alsa stopped (tx=%u rx=%u)", s_tx, s_rx);
}

int peer_alsa_running(void)
{
    return s_running;
}

uint32_t peer_alsa_tx_packets(void)
{
    return s_tx;
}

uint32_t peer_alsa_rx_packets(void)
{
    return s_rx;
}
