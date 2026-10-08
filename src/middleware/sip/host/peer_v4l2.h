/*
 * peer_v4l2.h - Ubuntu 侧的真实摄像头视频源（V4L2 MJPEG）
 *
 * 打开 /dev/video*，协商 MJPEG 输出（优先请求 want_w x want_h），MMAP 流式
 * 采集，按 fps 经 sip_jpeg_rtp 发送。无摄像头/无 MJPEG 格式时返回 -1，
 * 调用方可回退到 peer_vidsrc 的帧文件模式。
 */
#ifndef PEER_V4L2_H
#define PEER_V4L2_H

#include <stdint.h>

int peer_v4l2_start(const char *dev, int want_w, int want_h,
                    const char *remote_ip, uint16_t remote_port,
                    uint16_t rtp_local_port, int fps);
void peer_v4l2_stop(void);
int peer_v4l2_running(void);

uint32_t peer_v4l2_frames_sent(void);
int peer_v4l2_current_size(int *w, int *h);

#endif /* PEER_V4L2_H */
