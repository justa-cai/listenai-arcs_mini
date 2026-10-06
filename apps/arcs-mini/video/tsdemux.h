/*
 * tsdemux.h - MPEG-TS 解复用（拉流播放用，最小实现）
 *
 * 输入：任意长度分片的 TS 字节流（网络 chunked 接收直通即可）。
 * 输出：H264 AnnexB ES 分段（按 PES 边界切开，每段带 PTS）与
 *       AAC PES 载荷（内含 ADTS 帧），经回调送出。
 *
 * 实现：188 字节包重新同步、PAT→PMT 解析（H264=0x1B，AAC ADTS=0x0F）、
 * 每路 PID 的 PES 头解析与载荷重组（PUSI 开新 PES，上一段即完整）。
 * 多 section 的 PAT/PMT、continuity_counter、PCR、加扰流均不处理。
 */
#ifndef TSDEMUX_H
#define TSDEMUX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 回调返回非 0 将中断本次 feed（错误向上传播） */
typedef struct {
    /* H264 ES 分段（PES 载荷，AnnexB 字节流的连续片段） */
    int (*on_h264)(void *user, const uint8_t *data, size_t len, uint64_t pts);
    /* AAC 载荷（一个 PES 的内容，含整数个或需拼接的 ADTS 帧） */
    int (*on_aac)(void *user, const uint8_t *data, size_t len, uint64_t pts);
    /* PMT 完成（拿到两路 PID 后回调一次，可选） */
    void (*on_ready)(void *user, uint16_t h264_pid, uint16_t aac_pid);
} tsdemux_cb_t;

typedef struct {
    const tsdemux_cb_t *cb;
    void *user;

    /* 输入重同步状态 */
    int sync;             /* 已确认包对齐 */
    uint8_t pkt[188];
    size_t pkt_fill;

    /* PAT/PMT */
    int have_pmt;
    uint16_t pmt_pid;
    uint16_t h264_pid;    /* 0 = 未分配 */
    uint16_t aac_pid;

    /* 每路 PES 重组 */
    uint8_t *vbuf;        /* 视频载荷累积（PES 间复用） */
    size_t vbuf_cap, vlen;
    uint64_t v_pts;
    int v_pes_open;
    uint8_t *abuf;        /* 音频载荷累积 */
    size_t abuf_cap, alen;
    uint64_t a_pts;
    int a_pes_open;
} tsdemux_t;

/*
 * 初始化。vbuf_cap/abuf_cap 为内部 PES 累积缓冲大小（视频建议 >= 单帧
 * PES 最大长度，240x240 低码率下 64KB 足够；音频 8KB）。缓冲由调用方
 * 提供（可放 PSRAM），内容归 tsdemux 管理。
 */
void tsdemux_init(tsdemux_t *t, const tsdemux_cb_t *cb, void *user,
                  uint8_t *vbuf, size_t vbuf_cap, uint8_t *abuf,
                  size_t abuf_cap);

/* 喂入任意长度分片。返回 0；负值 = 回调要求中断（原样传回）。 */
int tsdemux_feed(tsdemux_t *t, const uint8_t *data, size_t len);

/* 复位到初始状态（断线重连时重用实例） */
void tsdemux_reset(tsdemux_t *t);

#ifdef __cplusplus
}
#endif

#endif /* TSDEMUX_H */
