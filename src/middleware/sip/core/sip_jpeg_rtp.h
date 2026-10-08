/*
 * sip_jpeg_rtp.h - MJPEG over RTP（RFC 2435 的自研子集）
 *
 * 约定（两端都是本 SDK，协商一致即成立，写死在两端）：
 *  - payload type 取静态 26（JPEG/90000），无需 rtpmap 协商；
 *  - 每个 RTP 净荷 = RFC 2435 主头 8B + JPEG 数据切片；
 *    Type=0（无重启标记），不带量化表头；
 *  - **Q=255 表示净荷拼起来是完整 JFIF 流**（tiny_jpeg/libjpeg/硬件解码器
 *    都能直接吃），不做 RFC 2435 的量化表重建 —— 每帧表开销约 132B，
 *    在 15KB 级的帧里可忽略；
 *  - 同一帧的所有分片共享同一 RTP 时间戳（90kHz），末片 M=1；
 *  - width/height 编码在主头里（像素/8，>2040 会溢出 —— 本设备最大 320）。
 */
#ifndef SIP_JPEG_RTP_H
#define SIP_JPEG_RTP_H

#include <stdint.h>

#include "sip_rtp.h"

#define SIP_JPEG_PT 26u
#define SIP_JPEG_MAX_FRAME (96 * 1024u)   /* 重组缓冲上限 */
#define SIP_JPEG_FRAGMENT 1400u           /* 每片 JPEG 数据上限（MTU 安全值） */

/* 发送一整帧：按 SIP_JPEG_FRAGMENT 分片，共用 ts（90kHz，由调用者按帧推进）。
 * 返回发出的分片数，<0 失败。 */
int sip_jpeg_rtp_send_frame(sip_rtp_t *r, const uint8_t *jpeg, int len,
                            int width, int height, uint32_t ts);

/* 接收重组器：按 ts 分组、按 offset 填充；新 ts 到来而旧帧未完 -> 丢旧帧。 */
typedef struct {
    uint8_t buf[SIP_JPEG_MAX_FRAME];
    uint32_t filled;          /* 末片位置（offset+len 的最大值） */
    uint32_t expected_end;    /* 末片给出的总长（M=1 片的 offset+len） */
    uint32_t cur_ts;
    int have_ts;
    int width, height;
    int frame_marked;         /* 已收到 M=1 片 */
    int32_t last_frag_idx;    /* 末片序号（不对齐切片时为 -1） */
    uint32_t frag_total;      /* 到齐的不同序号分片数 */
    uint8_t frag_seen[SIP_JPEG_MAX_FRAME / SIP_JPEG_FRAGMENT + 2u];
    uint32_t frames_ok, frames_drop, frag_count;
} sip_jpeg_rx_t;

/*
 * 喂一个完整 RTP 包（含 12 字节头）。
 * 返回 1：一帧完整，出参 frame/frame_len/w/h 指向 rx 内部缓冲（下次喂包前有效）；
 * 返回 0：分片已吸收，等后续；-1：包非法或被丢弃（统计已更新）。
 */
int sip_jpeg_rx_feed(sip_jpeg_rx_t *rx, const uint8_t *pkt, int len,
                     uint8_t expect_pt, const uint8_t **frame, uint32_t *frame_len,
                     int *w, int *h);

#endif /* SIP_JPEG_RTP_H */
