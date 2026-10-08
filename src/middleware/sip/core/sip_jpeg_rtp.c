/*
 * sip_jpeg_rtp.c - MJPEG over RTP 的分片与重组
 *
 * 主头（RFC 2435 4.1）：
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | type-specific | fragment offset (3B) |   type   |Q| width |height|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * 重组完整性：本 SDK 的发送端永远按 1400B 对齐切片，因此用片序号位图
 * 精确判洞；遇到不对齐的外来发送端退化为"末片位置 = 累计长度"校验。
 */
#include <string.h>

#include "sip_jpeg_rtp.h"

#define SIP_JPEG_FRAG_SLOTS ((SIP_JPEG_MAX_FRAME / SIP_JPEG_FRAGMENT) + 2u)

int sip_jpeg_rtp_send_frame(sip_rtp_t *r, const uint8_t *jpeg, int len,
                            int width, int height, uint32_t ts)
{
    if (!r || !jpeg || len <= 0 || width <= 0 || height <= 0 ||
        width > 2040 || height > 2040) {
        return -1;
    }

    /* 12B RTP 头 + 8B JPEG 主头 + 数据切片，总长 1420 < MTU 安全值。 */
    uint8_t pkt[12 + 8 + SIP_JPEG_FRAGMENT];
    int sent = 0;
    int offset = 0;
    while (offset < len) {
        int chunk = len - offset;
        if (chunk > (int)SIP_JPEG_FRAGMENT) {
            chunk = (int)SIP_JPEG_FRAGMENT;
        }
        int last = (offset + chunk >= len);

        sip_rtp_build_header(r, pkt, SIP_JPEG_PT, last, ts);
        pkt[12] = 0;                                    /* type-specific */
        pkt[13] = (uint8_t)((offset >> 16) & 0xFF);     /* fragment offset */
        pkt[14] = (uint8_t)((offset >> 8) & 0xFF);
        pkt[15] = (uint8_t)(offset & 0xFF);
        pkt[16] = 0;                                    /* type 0：无重启标记 */
        pkt[17] = 255;                                  /* Q=255：完整 JFIF 约定 */
        pkt[18] = (uint8_t)(width / 8);
        pkt[19] = (uint8_t)(height / 8);
        memcpy(pkt + 20, jpeg + offset, (size_t)chunk);

        if (sip_rtp_send_raw(r, pkt, 20 + chunk) <= 0) {
            return sent > 0 ? sent : -1;
        }
        sent++;
        offset += chunk;
    }
    return sent;
}

int sip_jpeg_rx_feed(sip_jpeg_rx_t *rx, const uint8_t *pkt, int len,
                     uint8_t expect_pt, const uint8_t **frame, uint32_t *frame_len,
                     int *w, int *h)
{
    if (!rx || !pkt || len <= 20) {
        return -1;
    }
    if ((pkt[1] & 0x7F) != expect_pt || (pkt[0] >> 6) != 2) {
        return -1;
    }
    int marker = (pkt[1] & 0x80) != 0;
    uint32_t ts = ((uint32_t)pkt[4] << 24) | ((uint32_t)pkt[5] << 16) |
                  ((uint32_t)pkt[6] << 8) | (uint32_t)pkt[7];
    uint32_t off = ((uint32_t)pkt[13] << 16) | ((uint32_t)pkt[14] << 8) | (uint32_t)pkt[15];
    uint32_t frag_len = (uint32_t)len - 20;

    if (off + frag_len > SIP_JPEG_MAX_FRAME) {
        rx->frames_drop++;
        return -1;
    }

    if (!rx->have_ts || ts != rx->cur_ts) {
        /* 新帧开始：上一帧若未交付则丢弃。 */
        if (rx->have_ts && !rx->frame_marked) {
            rx->frames_drop++;
        }
        rx->cur_ts = ts;
        rx->have_ts = 1;
        rx->filled = 0;
        rx->expected_end = 0;
        rx->frame_marked = 0;
        rx->frag_total = 0;
        rx->width = pkt[18] * 8;
        rx->height = pkt[19] * 8;
        memset(rx->frag_seen, 0, sizeof(rx->frag_seen));
    }

    memcpy(rx->buf + off, pkt + 20, frag_len);
    if (off % SIP_JPEG_FRAGMENT == 0 && off / SIP_JPEG_FRAGMENT < SIP_JPEG_FRAG_SLOTS) {
        uint32_t idx = off / SIP_JPEG_FRAGMENT;
        if (!rx->frag_seen[idx]) {
            rx->frag_seen[idx] = 1;
            rx->frag_total++;
        }
    }
    rx->frag_count++;
    if (marker) {
        rx->frame_marked = 1;
        rx->expected_end = off + frag_len;
        rx->last_frag_idx = (off % SIP_JPEG_FRAGMENT == 0)
                                ? (int32_t)(off / SIP_JPEG_FRAGMENT)
                                : -1; /* 不对齐：退化为长度校验 */
    }

    int complete = 0;
    if (rx->frame_marked && rx->expected_end > 0) {
        if (rx->last_frag_idx >= 0) {
            /* 精确判洞：0..last 全部到齐（首片 offset 必为 0）。 */
            complete = (rx->last_frag_idx == (int32_t)rx->frag_total - 1);
        } else {
            complete = (rx->frag_count >= rx->frag_total); /* 长度兜底 */
        }
    }

    if (complete) {
        rx->frames_ok++;
        if (frame) {
            *frame = rx->buf;
        }
        if (frame_len) {
            *frame_len = rx->expected_end;
        }
        if (w) {
            *w = rx->width;
        }
        if (h) {
            *h = rx->height;
        }
        rx->have_ts = 0; /* 交付后复位，下一包开新帧 */
        return 1;
    }
    return 0;
}
