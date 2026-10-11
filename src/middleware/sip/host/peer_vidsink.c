/*
 * peer_vidsink.c - 视频接收落盘 + SDL 窗口显示
 *
 * 帧写到 tmp/sip_host/frame_<n>.jpg，n 取模滚动（默认保留 8 帧历史，
 * 用 ls -t 排序即可观察刷新）。落盘路径相对当前工作目录，产物不进源码树。
 *
 * -W 时另开 SDL 窗口实时显示设备上行画面（libjpeg 解码 -> RGB24
 * texture）。窗口创建/渲染/销毁都在收帧线程（SDL 线程约束）；
 * 点 X 关窗自动降级回纯落盘模式。
 */
#define _TAG "peer_vidsink"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sip_osal.h"
#include "sip_rtp.h"
#include "sip_jpeg_rtp.h"

#include "peer_vidsink.h"

#ifdef VIDSINK_SDL
#include <SDL.h>
#include <jpeglib.h>
#include <setjmp.h>

static SDL_Window *s_win;
static SDL_Renderer *s_rend;
static SDL_Texture *s_tex;
static int s_tex_w, s_tex_h;

/* libjpeg 默认错误处理会 exit() 整个进程 —— 一张坏帧（重组边缘情况，
 * 曾实测 "Not a JPEG file: starts with 0xb9 0xff"）就把 peer 炸掉，
 * 设备侧还挂在通话里。这里用 setjmp 恢复，坏帧跳过。 */
struct sdl_jpeg_err {
    struct jpeg_error_mgr base;
    jmp_buf jb;
};

static void sdl_jpeg_error_exit(j_common_ptr c)
{
    struct sdl_jpeg_err *e = (struct sdl_jpeg_err *)c->err;
    longjmp(e->jb, 1);
}

static void sdl_cleanup(void)
{
    if (s_tex) {
        SDL_DestroyTexture(s_tex);
        s_tex = NULL;
    }
    if (s_rend) {
        SDL_DestroyRenderer(s_rend);
        s_rend = NULL;
    }
    if (s_win) {
        SDL_DestroyWindow(s_win);
        s_win = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        sip_osal_log(SIP_LOG_WARN, _TAG, "SDL init failed: %s (window off)", SDL_GetError());
        return;
    }
    s_win = SDL_CreateWindow("sip_peer - device uplink", 80, 80, 240, 320,
                             SDL_WINDOW_RESIZABLE);
    if (!s_win) {
        sip_osal_log(SIP_LOG_WARN, _TAG, "SDL window failed: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    s_rend = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED);
    if (!s_rend) {
        s_rend = SDL_CreateRenderer(s_win, -1, 0);
    }
    if (!s_rend) {
        sip_osal_log(SIP_LOG_WARN, _TAG, "SDL renderer failed: %s", SDL_GetError());
        sdl_cleanup();
        return;
    }
    SDL_SetRenderDrawColor(s_rend, 0, 0, 0, 255);
    sip_osal_log(SIP_LOG_INFO, _TAG, "SDL window on");
}

static void sdl_show_jpeg(const uint8_t *jpg, uint32_t len)
{
    if (!s_win || !s_rend) {
        return;
    }
    struct jpeg_decompress_struct d;
    struct sdl_jpeg_err jerr;
    d.err = jpeg_std_error(&jerr.base);
    jerr.base.error_exit = sdl_jpeg_error_exit;
    jpeg_create_decompress(&d);
    if (setjmp(jerr.jb)) {
        jpeg_destroy_decompress(&d);
        return;
    }
    jpeg_mem_src(&d, (unsigned char *)jpg, len);
    if (jpeg_read_header(&d, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&d);
        return;
    }
    d.out_color_space = JCS_RGB;
    jpeg_start_decompress(&d);
    int w = (int)d.output_width, h = (int)d.output_height;
    if (w <= 0 || h <= 0 || w > 1920 || h > 1920) {
        jpeg_destroy_decompress(&d);
        return;
    }
    if (!s_tex || s_tex_w != w || s_tex_h != h) {
        if (s_tex) {
            SDL_DestroyTexture(s_tex);
        }
        s_tex = SDL_CreateTexture(s_rend, SDL_PIXELFORMAT_RGB24,
                                  SDL_TEXTUREACCESS_STREAMING, w, h);
        s_tex_w = w;
        s_tex_h = h;
    }
    if (!s_tex) {
        jpeg_destroy_decompress(&d);
        return;
    }
    void *pix;
    int pitch;
    if (SDL_LockTexture(s_tex, NULL, &pix, &pitch) == 0) {
        while (d.output_scanline < (unsigned)h) {
            unsigned char *rows[1] =
                {(unsigned char *)pix + (size_t)d.output_scanline * pitch};
            jpeg_read_scanlines(&d, rows, 1);
        }
        SDL_UnlockTexture(s_tex);
    }
    jpeg_finish_decompress(&d);
    jpeg_destroy_decompress(&d);

    SDL_RenderClear(s_rend);
    /* 设备 MINI 板摄像头旋转安装，采集链路的 90°CW 补偿对通话视角
     * 差 180°（用户实测）—— 在显示层转回来。 */
    SDL_RenderCopyEx(s_rend, s_tex, NULL, NULL, 180.0, NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(s_rend);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            sip_osal_log(SIP_LOG_WARN, _TAG, "window closed, fall back to file sink");
            sdl_cleanup();
            return;
        }
    }
}
#endif /* VIDSINK_SDL */

#define VIDSINK_KEEP 8u

static sip_rtp_t s_rtp;
static sip_jpeg_rx_t *s_rx;
static pthread_t s_thread;
static volatile int s_running;
static volatile int s_window;
static volatile uint32_t s_ok, s_drop;

static void *vidsink_thread(void *arg)
{
    (void)arg;
    uint8_t pkt[1500];
    mkdir("tmp", 0755);
    mkdir("tmp/sip_host", 0755);
#ifdef VIDSINK_SDL
    if (s_window) {
        sdl_init();
    }
#endif

    while (s_running) {
        int n = sip_osal_udp_recv(s_rtp.fd, pkt, sizeof(pkt), 50, NULL, NULL);
        if (n <= 0) {
            continue;
        }
        const uint8_t *frame;
        uint32_t len;
        int w, h;
        int rc = sip_jpeg_rx_feed(s_rx, pkt, n, SIP_JPEG_PT, &frame, &len, &w, &h);
        if (rc == 1) {
            if (len < 2 || frame[0] != 0xFF || frame[1] != 0xD8) {
                s_drop++;
                continue; /* 重组出的坏帧（SOI 都不对），别喂解码器 */
            }
#ifdef VIDSINK_SDL
            if (s_window) {
                sdl_show_jpeg(frame, len);
            }
#endif
            static uint32_t seq;
            char path[64];
            snprintf(path, sizeof(path), "tmp/sip_host/frame_%u.jpg", seq % VIDSINK_KEEP);
            seq++;
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(frame, 1, len, f);
                fclose(f);
            }
            s_ok++;
            if ((s_ok % 10u) == 1u) {
                sip_osal_log(SIP_LOG_INFO, _TAG, "frame #%u %u bytes %dx%d -> %s",
                             s_ok, len, w, h, path);
            }
        }
    }
    return NULL;
}

void peer_vidsink_show_window(int enable)
{
    s_window = enable;
}

int peer_vidsink_start(const char *remote_ip, uint16_t remote_port,
                       uint16_t rtp_local_port)
{
    if (s_running) {
        return 0;
    }
    if (sip_rtp_open(&s_rtp, rtp_local_port, SIP_JPEG_PT) != 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "rtp bind %u failed", rtp_local_port);
        return -1;
    }
    sip_rtp_set_peer(&s_rtp, remote_ip, remote_port);

    s_rx = malloc(sizeof(*s_rx));
    if (!s_rx) {
        sip_rtp_close(&s_rtp);
        return -1;
    }
    memset(s_rx, 0, sizeof(*s_rx));

    s_ok = s_drop = 0;
    s_running = 1;
    pthread_create(&s_thread, NULL, vidsink_thread, NULL);
    sip_osal_log(SIP_LOG_INFO, _TAG, "video sink %s:%u -> tmp/sip_host/",
                 remote_ip, remote_port);
    return 0;
}

void peer_vidsink_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = 0;
    sip_rtp_close(&s_rtp);
    pthread_join(s_thread, NULL);
#ifdef VIDSINK_SDL
    sdl_cleanup(); /* SDL 对象在收帧线程创建，此处 join 后统一销毁 */
#endif
    s_drop = s_rx->frames_drop;
    free(s_rx);
    s_rx = NULL;
    sip_osal_log(SIP_LOG_INFO, _TAG, "stopped (ok=%u drop=%u)", s_ok, s_drop);
}

int peer_vidsink_running(void)
{
    return s_running;
}

uint32_t peer_vidsink_frames_ok(void)
{
    return s_ok;
}

uint32_t peer_vidsink_frames_drop(void)
{
    return s_rx ? s_rx->frames_drop : s_drop;
}
