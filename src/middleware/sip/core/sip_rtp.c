/*
 * sip_rtp.c - RTP 收发实现
 *
 * 头部固定 12B（V=2, 无扩展/填充/CSRC）。音频 M=0；视频末片 M=1（jpeg_rtp 用）。
 * 时间戳：音频 8000Hz 时钟每 20ms 包 +160；视频 90000Hz 由上层按帧推进
 * （sip_rtp_send_video_ts 由 jpeg 模块调用，这里只提供裸发送）。
 */
#include <stdio.h>
#include <string.h>

#include "sip_rtp.h"

int sip_rtp_open(sip_rtp_t *r, uint16_t local_port, uint8_t pt)
{
    memset(r, 0, sizeof(*r));
    r->fd = sip_osal_udp_open(local_port);
    if (r->fd < 0) {
        return -1;
    }
    r->local_port = local_port;
    r->pt = pt;
    r->seq = (uint16_t)(sip_osal_rand() & 0xFFFF);
    r->ssrc = sip_osal_rand();
    return 0;
}

void sip_rtp_set_peer(sip_rtp_t *r, const char *ip, uint16_t port)
{
    snprintf(r->peer_ip, sizeof(r->peer_ip), "%s", ip);
    r->peer_port = port;
    r->peer_locked = 0; /* 显式设置不算"学习到"，仍等首包锁定 */
}

void sip_rtp_close(sip_rtp_t *r)
{
    if (r->fd >= 0) {
        sip_osal_udp_close(r->fd);
    }
    r->fd = -1;
}

int sip_rtp_build_header(sip_rtp_t *r, uint8_t *out, uint8_t pt, int marker,
                         uint32_t ts)
{
    out[0] = 0x80; /* V=2, P=0, X=0, CC=0 */
    out[1] = (uint8_t)((marker ? 0x80 : 0x00) | (pt & 0x7F));
    out[2] = (uint8_t)(r->seq >> 8);
    out[3] = (uint8_t)(r->seq & 0xFF);
    out[4] = (uint8_t)(ts >> 24);
    out[5] = (uint8_t)(ts >> 16);
    out[6] = (uint8_t)(ts >> 8);
    out[7] = (uint8_t)(ts & 0xFF);
    out[8] = (uint8_t)(r->ssrc >> 24);
    out[9] = (uint8_t)(r->ssrc >> 16);
    out[10] = (uint8_t)(r->ssrc >> 8);
    out[11] = (uint8_t)(r->ssrc & 0xFF);
    r->seq++; /* 每个包序号 +1（同帧分片也各自递增） */
    return 12;
}

int sip_rtp_send_raw(sip_rtp_t *r, const uint8_t *pkt, int len)
{
    if (r->fd < 0 || !r->peer_ip[0] || r->peer_port == 0) {
        return -1;
    }
    int sent = sip_osal_udp_send(r->fd, r->peer_ip, r->peer_port, pkt, (size_t)len);
    if (sent > 0) {
        r->tx_pkts++;
        r->tx_bytes += (uint32_t)sent;
    }
    return sent;
}

int sip_rtp_send_audio(sip_rtp_t *r, const uint8_t *payload, int n)
{
    if (n <= 0 || n > 512) {
        return -1;
    }
    uint8_t pkt[12 + 512];
    sip_rtp_build_header(r, pkt, r->pt, 0, r->ts);
    r->ts += (uint32_t)n; /* 8k 时钟：字节数即样本数 */
    memcpy(pkt + 12, payload, (size_t)n);
    return sip_rtp_send_raw(r, pkt, 12 + n);
}

int sip_rtp_recv_audio(sip_rtp_t *r, uint8_t *payload, int cap, uint32_t timeout_ms)
{
    uint8_t pkt[1500];
    char from_ip[16];
    uint16_t from_port = 0;
    int n = sip_osal_udp_recv(r->fd, pkt, sizeof(pkt), timeout_ms,
                              from_ip, &from_port);
    if (n <= 12) {
        return n > 0 ? 0 : n; /* 太短的包当噪声丢弃；n==0 为超时 */
    }
    if ((pkt[1] & 0x7F) != r->pt) {
        return 0; /* PT 不匹配（对端还没按 answer 切换），丢弃 */
    }
    /* 对称 RTP：首包锁定来源。 */
    if (!r->peer_locked) {
        if (!r->peer_ip[0] || strcmp(r->peer_ip, from_ip) == 0) {
            snprintf(r->peer_ip, sizeof(r->peer_ip), "%s", from_ip);
            r->peer_port = from_port;
            r->peer_locked = 1;
        } else {
            return 0; /* 既定目标外的来源，丢弃 */
        }
    } else if (strcmp(r->peer_ip, from_ip) != 0 || r->peer_port != from_port) {
        return 0;
    }

    uint16_t seq = (uint16_t)((pkt[2] << 8) | pkt[3]);
    if (r->rx_have_seq) {
        uint16_t diff = (uint16_t)(seq - r->rx_last_seq);
        if (diff > 1 && diff < 0x8000) {
            r->rx_loss_events++;
        }
    }
    r->rx_last_seq = seq;
    r->rx_have_seq = 1;
    r->rx_pkts++;
    r->rx_bytes += (uint32_t)n;

    int plen = n - 12;
    if (plen > cap) {
        plen = cap;
    }
    memcpy(payload, pkt + 12, (size_t)plen);
    return plen;
}
