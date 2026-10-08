/*
 * peer_alsa.h - Ubuntu 对端的 ALSA 全双工音频（8kHz 单声道 S16）
 *
 * 两个线程：capture（arecord 方向）读麦克风 → G.711 → RTP 发送；
 * playback（aplay 方向）RTP 接收 → 解码 → 写扬声器。设备用 "default"
 * （plug 层自动做重采样/格式转换，不需要 hw: 直连）。
 */
#ifndef PEER_ALSA_H
#define PEER_ALSA_H

#include <stdint.h>

/* 启动音频线程。rtp_local_port 为本端接收端口，remote 为对端（设备）。 */
int peer_alsa_start(int audio_pt, const char *remote_ip, uint16_t remote_port,
                    uint16_t rtp_local_port);
void peer_alsa_stop(void);
int peer_alsa_running(void);

uint32_t peer_alsa_tx_packets(void);
uint32_t peer_alsa_rx_packets(void);

#endif /* PEER_ALSA_H */
