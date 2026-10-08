/*
 * peer_vidsrc.h - Ubuntu 侧的视频发送源（设备模拟 / 主机摄像头共用）
 *
 * 从 JPEG 帧目录循环读文件，按目标帧率经 sip_jpeg_rtp 分片发送 —— 与设备端
 * sip_vid 任务（相机采集 → 软件 JPEG → 分片）同一发送逻辑，用于无设备时在
 * Ubuntu 上验证"设备→对端"的上行链路。
 */
#ifndef PEER_VIDSRC_H
#define PEER_VIDSRC_H

#include <stdint.h>

/* frames_dir：存放 frame_*.jpg 的目录；remote 为对端（设备/对端程序）。 */
int peer_vidsrc_start(const char *frames_dir, int width, int height,
                      const char *remote_ip, uint16_t remote_port,
                      uint16_t rtp_local_port, int fps);
void peer_vidsrc_stop(void);
int peer_vidsrc_running(void);

uint32_t peer_vidsrc_frames_sent(void);

#endif /* PEER_VIDSRC_H */
