/*
 * video_player.h - 在线 H264+AAC 拉流播放服务
 *
 * 播放链路：HTTP(chunked) 拉取 MPEG-TS → tsdemux 解复用 →
 *  - H264 ES → h264dec 纯C软解 → YUV420P→RGB565 → LVGL 视频页显示
 *  - AAC ADTS → aacdec → PCM → app_player 流式输出
 * 不落任何存储。URL 由调用方给出（如 http://192.168.1.10:8000/live.ts）。
 */
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 开始播放（异步：立即返回，网络/解码在后台任务进行）。
 * 会在 UI 线程创建/切换到视频页面。url 会被拷贝，可传栈上字符串。
 * 返回 0 成功启动，负值失败。
 */
int video_player_start(const char *url);

/* 停止播放并返回原页面。幂等。 */
int video_player_stop(void);

/* 当前是否在播放 */
bool video_player_is_playing(void);

/* 状态统计（用于 shell/MCP 查询） */
typedef struct {
    bool playing;
    uint32_t frames;        /* 已解码视频帧数 */
    uint32_t display_frames;/* 已送显帧数 */
    uint32_t audio_frames;  /* 已解码音频帧数 */
    uint32_t http_hops;     /* 断线重连次数 */
    uint32_t drop_chunks;   /* 因缓冲满丢弃的网络分片 */
    uint32_t last_decode_ms;/* 最近一帧解码耗时 */
    uint32_t last_csc_ms;   /* 最近一帧色彩转换耗时 */
    int video_width, video_height;
    uint32_t declared_fps;  /* SPS 声明帧率（time_scale/(2*nuit)），0=未声明 */
} video_player_status_t;

void video_player_get_status(video_player_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_PLAYER_H */
