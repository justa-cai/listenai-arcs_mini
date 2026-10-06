/*
 * tsdemux.c - MPEG-TS 解复用实现，结构见 tsdemux.h
 *
 * 参考 ISO/IEC 13818-1：2.4.3.2（TS 包）、2.4.4.3（PES 包）、
 * 2.4.4.4/2.4.4.8（PAT/PMT）。只覆盖 ffmpeg 默认 mpegts 输出会用到的
 * 路径：单 section PAT/PMT、非加扰流、PES_length==0 的视频流。
 */
#include <string.h>

#include "tsdemux.h"

#define TS_PKT_SIZE 188
#define TS_PID_PAT 0x0000
#define TS_PID_NULL 0x1FFF
#define TS_STREAM_H264 0x1B
#define TS_STREAM_AAC_ADTS 0x0F

void tsdemux_init(tsdemux_t *t, const tsdemux_cb_t *cb, void *user,
                  uint8_t *vbuf, size_t vbuf_cap, uint8_t *abuf,
                  size_t abuf_cap) {
    memset(t, 0, sizeof(*t));
    t->cb = cb;
    t->user = user;
    t->vbuf = vbuf;
    t->vbuf_cap = vbuf_cap;
    t->abuf = abuf;
    t->abuf_cap = abuf_cap;
}

void tsdemux_reset(tsdemux_t *t) {
    uint8_t *vbuf = t->vbuf, *abuf = t->abuf;
    size_t vcap = t->vbuf_cap, acap = t->abuf_cap;
    const tsdemux_cb_t *cb = t->cb;
    void *user = t->user;
    memset(t, 0, sizeof(*t));
    t->vbuf = vbuf;
    t->abuf = abuf;
    t->vbuf_cap = vcap;
    t->abuf_cap = acap;
    t->cb = cb;
    t->user = user;
}

/* 33-bit PTS（5 字节分散编码，ISO/IEC 13818-1 2.4.3.7） */
static uint64_t parse_pts(const uint8_t *p) {
    uint64_t pts;
    pts = ((uint64_t)(p[0] >> 1) & 0x07) << 30;
    pts |= ((uint64_t)p[1] << 22) | ((uint64_t)(p[2] >> 1) << 15);
    pts |= ((uint64_t)p[3] << 7) | ((uint64_t)(p[4] >> 1));
    return pts;
}

/* PAT/PMT section 解析。返回 0。仅在 PUSI=1 的 TS 包里找 table 头。 */
static void parse_pat(tsdemux_t *t, const uint8_t *p, size_t len) {
    /* p 指向 pointer_field */
    if (len < 8) return;
    size_t off = 1 + p[0]; /* 跳到 section 开头 */
    if (off + 8 > len) return;
    const uint8_t *s = p + off;
    if (s[0] != 0x00) return; /* table_id PAT */
    size_t sect_len = ((size_t)(s[1] & 0x0F) << 8) | s[2];
    if (off + 3 + sect_len > len) return;
    size_t n = off + 8; /* 跳过 section 头(语法版本等) */
    size_t end = off + 3 + sect_len - 4; /* 去 CRC32 */
    while (n + 4 <= end) {
        uint16_t prog = ((uint16_t)p[n] << 8) | p[n + 1];
        uint16_t pid = (((uint16_t)p[n + 2] & 0x1F) << 8) | p[n + 3];
        if (prog != 0) {
            t->pmt_pid = pid;
            return; /* 取第一个节目 */
        }
        n += 4;
    }
}

static void parse_pmt(tsdemux_t *t, const uint8_t *p, size_t len) {
    if (len < 8) return;
    size_t off = 1 + p[0];
    if (off + 8 > len) return;
    const uint8_t *s = p + off;
    if (s[0] != 0x02) return; /* table_id PMT */
    size_t sect_len = ((size_t)(s[1] & 0x0F) << 8) | s[2];
    if (off + 3 + sect_len > len) return;
    /* s[8..9] PCR PID, s[10..11] program_info_len */
    size_t prog_info_len = ((size_t)(s[10] & 0x0F) << 8) | s[11];
    size_t n = off + 12 + prog_info_len;
    size_t end = off + 3 + sect_len - 4;
    while (n + 5 <= end) {
        uint8_t type = p[n];
        uint16_t pid = (((uint16_t)p[n + 1] & 0x1F) << 8) | p[n + 2];
        size_t es_len = ((size_t)(p[n + 3] & 0x0F) << 8) | p[n + 4];
        if (type == TS_STREAM_H264 && t->h264_pid == 0) {
            t->h264_pid = pid;
        } else if (type == TS_STREAM_AAC_ADTS && t->aac_pid == 0) {
            t->aac_pid = pid;
        }
        n += 5 + es_len;
    }
    if (t->h264_pid || t->aac_pid) {
        t->have_pmt = 1;
        if (t->cb->on_ready) {
            t->cb->on_ready(t->user, t->h264_pid, t->aac_pid);
        }
    }
}

/* PES 载荷重组输出（视频/音频各一套小状态，逻辑相同） */
typedef struct {
    uint8_t *buf;
    size_t cap, len;
    uint64_t pts;
    int open;
} pes_acc_t;

/* PUSI=1 的 TS 包：解析 PES 头，先吐出上一段累积 */
static int pes_start(tsdemux_t *t, pes_acc_t *acc, const uint8_t *payload,
                     size_t plen, int is_video) {
    /* PES 最小头 9 字节：code(3) stream_id(1) len(2) flags(2) hdrlen(1) */
    if (plen < 9 || payload[0] != 0 || payload[1] != 0 || payload[2] != 1) {
        acc->open = 0;
        return 0;
    }
    /* 先输出上一段（PES 之间边界 = 完整） */
    if (acc->open && acc->len > 0) {
        int r;
        if (is_video) {
            r = t->cb->on_h264 ? t->cb->on_h264(t->user, acc->buf, acc->len,
                                                acc->pts)
                               : 0;
        } else {
            r = t->cb->on_aac ? t->cb->on_aac(t->user, acc->buf, acc->len,
                                              acc->pts)
                              : 0;
        }
        if (r != 0) return r;
    }
    acc->len = 0;
    acc->pts = 0;

    uint8_t flags = payload[7];
    size_t hdr = 9 + payload[8];
    if (flags & 0x80) { /* PTS_DTS_flags 有 PTS */
        if (plen < hdr) {
            acc->open = 0;
            return 0;
        }
        acc->pts = parse_pts(payload + 9);
    }
    if (hdr > plen) {
        acc->open = 0;
        return 0;
    }
    size_t n = plen - hdr;
    if (n > acc->cap) n = acc->cap; /* 超容丢弃尾部（容量应足够） */
    memcpy(acc->buf, payload + hdr, n);
    acc->len = n;
    acc->open = 1;
    return 0;
}

static void pes_cont(pes_acc_t *acc, const uint8_t *payload, size_t plen) {
    if (!acc->open) return;
    size_t n = acc->cap - acc->len;
    if (n > plen) n = plen;
    memcpy(acc->buf + acc->len, payload, n);
    acc->len += n;
}

static int handle_ts_pkt(tsdemux_t *t, const uint8_t *pkt) {
    if (pkt[0] != 0x47) return 0;
    int pusi = (pkt[1] >> 6) & 1;
    uint16_t pid = (((uint16_t)(pkt[1] & 0x1F)) << 8) | pkt[2];
    int afc = (pkt[3] >> 4) & 3;
    if (pid == TS_PID_NULL) return 0;

    const uint8_t *payload = pkt + 4;
    size_t plen = TS_PKT_SIZE - 4;
    if (afc & 2) { /* adaptation field */
        size_t af_len = pkt[4];
        if (af_len + 1 > plen) return 0;
        payload = pkt + 5 + af_len;
        plen = TS_PKT_SIZE - 5 - af_len;
    }
    if (plen == 0 || (afc & 1) == 0) return 0; /* 无载荷 */

    if (pid == TS_PID_PAT) {
        if (pusi) parse_pat(t, payload, plen);
        return 0;
    }
    if (!t->have_pmt) {
        if (t->pmt_pid && pid == t->pmt_pid && pusi) {
            parse_pmt(t, payload, plen);
        }
        return 0;
    }

    pes_acc_t vacc = {t->vbuf, t->vbuf_cap, t->vlen, t->v_pts, t->v_pes_open};
    pes_acc_t aacc = {t->abuf, t->abuf_cap, t->alen, t->a_pts, t->a_pes_open};
    int r = 0;
    if (t->h264_pid && pid == t->h264_pid) {
        if (pusi) {
            r = pes_start(t, &vacc, payload, plen, 1);
        } else {
            pes_cont(&vacc, payload, plen);
        }
    } else if (t->aac_pid && pid == t->aac_pid) {
        if (pusi) {
            r = pes_start(t, &aacc, payload, plen, 0);
        } else {
            pes_cont(&aacc, payload, plen);
        }
    }
    t->vlen = vacc.len;
    t->v_pts = vacc.pts;
    t->v_pes_open = vacc.open;
    t->alen = aacc.len;
    t->a_pts = aacc.pts;
    t->a_pes_open = aacc.open;
    return r;
}

int tsdemux_feed(tsdemux_t *t, const uint8_t *data, size_t len) {
    while (len > 0) {
        if (t->pkt_fill == 0 && t->sync) {
            /* 已对齐：直接按包消化 */
            if (data[0] != 0x47) {
                /* 失步：重新搜索 */
                t->sync = 0;
                continue;
            }
            size_t n = len - len % TS_PKT_SIZE;
            for (size_t i = 0; i + TS_PKT_SIZE <= len; i += TS_PKT_SIZE) {
                if (data[i] != 0x47) {
                    t->sync = 0;
                    break;
                }
                int r = handle_ts_pkt(t, data + i);
                if (r != 0) return r;
                n = i + TS_PKT_SIZE;
            }
            size_t used = n;
            /* 尾部不足一包留在半包缓冲 */
            size_t tail = len - used;
            if (tail) {
                memcpy(t->pkt, data + used, tail);
                t->pkt_fill = tail;
            }
            return 0;
        }
        if (t->pkt_fill > 0 || !t->sync) {
            /* 半包积累或失步搜索 */
            if (!t->sync) {
                const uint8_t *p = memchr(data, 0x47, len);
                if (!p) return 0;
                size_t skip = (size_t)(p - data);
                data += skip;
                len -= skip;
                t->sync = 1;
                t->pkt_fill = 0;
                continue;
            }
            size_t need = TS_PKT_SIZE - t->pkt_fill;
            size_t take = len < need ? len : need;
            memcpy(t->pkt + t->pkt_fill, data, take);
            t->pkt_fill += take;
            data += take;
            len -= take;
            if (t->pkt_fill == TS_PKT_SIZE) {
                int r = handle_ts_pkt(t, t->pkt);
                t->pkt_fill = 0;
                if (r != 0) return r;
            }
        }
    }
    return 0;
}
