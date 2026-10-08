/*
 * peer_vidsink.h - Ubuntu 对端的视频接收（设备 → 主机方向）
 *
 * 独立线程接收视频 RTP，用 core 的 sip_jpeg_rx 重组；完整帧落盘到
 * tmp/sip_host/frame_N.jpg（滚动覆盖最近 8 帧，避免撑满磁盘）。
 */
#ifndef PEER_VIDSINK_H
#define PEER_VIDSINK_H

#include <stdint.h>

int peer_vidsink_start(const char *remote_ip, uint16_t remote_port,
                       uint16_t rtp_local_port);
void peer_vidsink_stop(void);
int peer_vidsink_running(void);

uint32_t peer_vidsink_frames_ok(void);
uint32_t peer_vidsink_frames_drop(void);

#endif /* PEER_VIDSINK_H */
