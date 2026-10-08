/*
 * sip_rtp.h - RTP 收发（RFC 3550 子集，无 RTCP）
 *
 * 一个 sip_rtp_t 对应一个媒体 socket（音频 5062 / 视频 5064）。
 * 对称 RTP：首次收到对端合法包后锁定发送目标，避免对端端口与 SDP 声明
 * 不一致时出现单向流。
 */
#ifndef SIP_RTP_H
#define SIP_RTP_H

#include <stdint.h>

#include "sip_osal.h"

typedef struct {
    int fd;
    uint16_t local_port;
    uint16_t seq;        /* 发送序号，每包 +1 */
    uint32_t ts;         /* 发送时间戳（音频 8k 时钟） */
    uint32_t ssrc;
    uint8_t pt;          /* 本端发送的 payload type */
    char peer_ip[16];
    uint16_t peer_port;
    int peer_locked;     /* 对称 RTP 锁定标志 */
    /* 统计（shell status 用） */
    uint32_t tx_pkts, rx_pkts, tx_bytes, rx_bytes;
    uint32_t rx_loss_events;   /* seq 跳变次数（近似丢包） */
    uint16_t rx_last_seq;
    int rx_have_seq;
} sip_rtp_t;

int sip_rtp_open(sip_rtp_t *r, uint16_t local_port, uint8_t pt);
void sip_rtp_set_peer(sip_rtp_t *r, const char *ip, uint16_t port);
void sip_rtp_close(sip_rtp_t *r);

/* 发送一包音频：g711 净荷 n 字节（20ms = 160），ts 自增 160。 */
int sip_rtp_send_audio(sip_rtp_t *r, const uint8_t *payload, int n);

/* 接收一包音频：净荷拷入 payload（cap 上限），返回长度；0=超时；-1=错误。
 * 只接受来自锁定/首个对端的 PT 匹配包，其余静默丢弃。 */
int sip_rtp_recv_audio(sip_rtp_t *r, uint8_t *payload, int cap, uint32_t timeout_ms);

/* 组 RTP 头（12B）到 out；供视频分片（sip_jpeg_rtp）复用同一序号空间。 */
int sip_rtp_build_header(sip_rtp_t *r, uint8_t *out, uint8_t pt, int marker,
                         uint32_t ts_advance);

/* 视频用：直接向已锁定的对端发送一个完整 UDP 包。 */
int sip_rtp_send_raw(sip_rtp_t *r, const uint8_t *pkt, int len);

#endif /* SIP_RTP_H */
