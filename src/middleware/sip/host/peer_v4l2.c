/*
 * peer_v4l2.c - V4L2 MJPEG 采集发送
 *
 * 只支持 MJPEG 输出（绝大多数 UVC 摄像头都有）：相机本身就是 JPEG 编码器，
 * 主机 CPU 零编码开销。分辨率协商：先试 want 尺寸，不成再用设备给出的任意
 * 尺寸 —— 注意设备端只接受 <=240x320 的帧，超过会整帧丢弃，所以这里
 * 请求的尺寸默认就是 240x320（本函数不做缩放）。
 */
#define _TAG "peer_v4l2"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <jpeglib.h>

#include "sip_osal.h"
#include "sip_rtp.h"
#include "sip_jpeg_rtp.h"

#include "peer_v4l2.h"

#define V4L2_BUF_COUNT 4

static int s_fd = -1;
static sip_rtp_t s_rtp;
static pthread_t s_thread;
static volatile int s_running;
static volatile uint32_t s_sent;
static int s_w, s_h;
static struct {
    void *start;
    size_t length;
} s_buffers[V4L2_BUF_COUNT];
static int s_buf_count;

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do {
        r = ioctl(fd, req, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

static int device_init(const char *dev, int want_w, int want_h)
{
    s_fd = open(dev, O_RDWR);
    if (s_fd < 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "open %s failed: %s", dev, strerror(errno));
        return -1;
    }

    struct v4l2_capability cap;
    if (xioctl(s_fd, VIDIOC_QUERYCAP, &cap) < 0 ||
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "%s is not a capture device", dev);
        return -1;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = want_w > 0 ? (uint32_t)want_w : 240;
    fmt.fmt.pix.height = want_h > 0 ? (uint32_t)want_h : 320;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(s_fd, VIDIOC_S_FMT, &fmt) < 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "S_FMT MJPEG failed: %s", strerror(errno));
        return -1;
    }
    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "device does not support MJPEG");
        return -1;
    }
    s_w = (int)fmt.fmt.pix.width;
    s_h = (int)fmt.fmt.pix.height;

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = V4L2_BUF_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(s_fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "REQBUFS failed: %s", strerror(errno));
        return -1;
    }
    s_buf_count = (int)req.count;
    for (int i = 0; i < s_buf_count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = (uint32_t)i;
        if (xioctl(s_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            sip_osal_log(SIP_LOG_ERR, _TAG, "QUERYBUF %d failed", i);
            return -1;
        }
        s_buffers[i].length = buf.length;
        s_buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, s_fd, buf.m.offset);
        if (s_buffers[i].start == MAP_FAILED) {
            sip_osal_log(SIP_LOG_ERR, _TAG, "mmap %d failed", i);
            return -1;
        }
    }

    for (int i = 0; i < s_buf_count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = (uint32_t)i;
        if (xioctl(s_fd, VIDIOC_QBUF, &buf) < 0) {
            sip_osal_log(SIP_LOG_ERR, _TAG, "QBUF %d failed", i);
            return -1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(s_fd, VIDIOC_STREAMON, &type) < 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "STREAMON failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static void device_deinit(void)
{
    if (s_fd >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        (void)xioctl(s_fd, VIDIOC_STREAMOFF, &type);
        for (int i = 0; i < s_buf_count; ++i) {
            if (s_buffers[i].start && s_buffers[i].start != MAP_FAILED) {
                munmap(s_buffers[i].start, s_buffers[i].length);
            }
        }
        close(s_fd);
        s_fd = -1;
    }
    s_buf_count = 0;
}


/*
 * 相机帧缩放：JPEG 解码 -> 最近邻缩放并信箱化（保持宽高比、黑边填充）
 * -> 重编码。相机往往给不出 <=240x320 的 MJPEG 分辨率，而设备端屏幕
 * 只有 240x320，超尺寸帧会被整帧丢弃 —— 必须在主机侧变换。
 */
#define SCALE_DST_W 240
#define SCALE_DST_H 320

static int scale_jpeg_frame(const uint8_t *in, uint32_t in_len,
                            uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    /* 解码 */
    struct jpeg_decompress_struct d;
    struct jpeg_error_mgr jerr;
    d.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&d);
    jpeg_mem_src(&d, (unsigned char *)in, in_len);
    if (jpeg_read_header(&d, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&d);
        return -1;
    }
    d.scale_num = 1;
    d.scale_denom = 1;
    jpeg_start_decompress(&d);
    int sw = (int)d.output_width;
    int sh = (int)d.output_height;
    if (sw <= 0 || sh <= 0 || d.output_components != 3) {
        jpeg_destroy_decompress(&d);
        return -1;
    }

    uint8_t *src = malloc((size_t)sw * sh * 3);
    if (!src) {
        jpeg_destroy_decompress(&d);
        return -1;
    }
    while (d.output_scanline < (JDIMENSION)sh) {
        unsigned char *rowp[1] = {src + (size_t)d.output_scanline * sw * 3};
        jpeg_read_scanlines(&d, rowp, 1);
    }
    jpeg_finish_decompress(&d);
    jpeg_destroy_decompress(&d);

    /* 目标尺寸内按宽高比取最大内接矩形，居中，其余填黑。 */
    int fw = SCALE_DST_W, fh = SCALE_DST_H;
    if ((long)sw * SCALE_DST_H > (long)sh * SCALE_DST_W) {
        fh = (int)(((long)sh * SCALE_DST_W + sw - 1) / sw);
    } else {
        fw = (int)(((long)sw * SCALE_DST_H + sh - 1) / sh);
    }
    int ox = (SCALE_DST_W - fw) / 2;
    int oy = (SCALE_DST_H - fh) / 2;

    uint8_t *dst = calloc(1, (size_t)SCALE_DST_W * SCALE_DST_H * 3);
    if (!dst) {
        free(src);
        return -1;
    }
    for (int y = 0; y < fh; ++y) {
        const uint8_t *srow = src + (size_t)(y * sh / fh) * sw * 3;
        uint8_t *drow = dst + (size_t)(oy + y) * SCALE_DST_W * 3 + (size_t)ox * 3;
        for (int x = 0; x < fw; ++x) {
            const uint8_t *p = srow + (size_t)(x * sw / fw) * 3;
            drow[x * 3 + 0] = p[0];
            drow[x * 3 + 1] = p[1];
            drow[x * 3 + 2] = p[2];
        }
    }
    free(src);

    /* 重编码 */
    struct jpeg_compress_struct c;
    c.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&c);
    unsigned char *outbuf = NULL;
    unsigned long outsize = 0;
    jpeg_mem_dest(&c, &outbuf, &outsize);
    c.image_width = SCALE_DST_W;
    c.image_height = SCALE_DST_H;
    c.input_components = 3;
    c.in_color_space = JCS_RGB;
    jpeg_set_defaults(&c);
    /* RGB 输入的 libjpeg 默认按 4:4:4 采样，设备 JPEG 硬解不支持 4:4:4
     * （实测解出规则条纹垃圾且返回成功）。显式切 YCbCr 让 libjpeg 采用
     * 默认的 4:2:0（h2v2/h1v1/h1v1）。 */
    jpeg_set_colorspace(&c, JCS_YCbCr);
    jpeg_set_quality(&c, 70, TRUE);
    jpeg_start_compress(&c, TRUE);
    while (c.next_scanline < (JDIMENSION)SCALE_DST_H) {
        unsigned char *rowp[1] = {dst + (size_t)c.next_scanline * SCALE_DST_W * 3};
        jpeg_write_scanlines(&c, rowp, 1);
    }
    jpeg_finish_compress(&c);
    jpeg_destroy_compress(&c);
    free(dst);

    int rc = -1;
    if (outsize > 0 && outsize <= out_cap) {
        memcpy(out, outbuf, outsize);
        *out_len = (uint32_t)outsize;
        rc = 0;
    }
    free(outbuf);
    return rc;
}

static void *v4l2_thread(void *arg)
{
    (void)arg;
    uint32_t interval = 1000u / 5u;
    uint32_t ts = 0;
    while (s_running) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(s_fd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EINTR) {
                continue;
            }
            sip_osal_log(SIP_LOG_ERR, _TAG, "DQBUF failed: %s", strerror(errno));
            break;
        }
        int idx = (int)buf.index;
        const uint8_t *send_buf = (const uint8_t *)s_buffers[idx].start;
        int send_len = (int)buf.bytesused;
        int send_w = s_w, send_h = s_h;
        if (s_w > 240 || s_h > 320) {
            /* 相机分辨率超过设备屏幕：缩放到 240x320 再发。 */
            static uint8_t scaled[240 * 320 * 3];
            uint32_t scaled_len = 0;
            if (scale_jpeg_frame(send_buf, (uint32_t)send_len, scaled,
                                 sizeof(scaled), &scaled_len) == 0) {
                send_buf = scaled;
                send_len = (int)scaled_len;
                send_w = 240;
                send_h = 320;
            } else {
                (void)xioctl(s_fd, VIDIOC_QBUF, &buf);
                continue; /* 缩放失败干脆不发（设备也会丢） */
            }
        }
        int frags = sip_jpeg_rtp_send_frame(&s_rtp, send_buf, send_len,
                                            send_w, send_h, ts);
        (void)xioctl(s_fd, VIDIOC_QBUF, &buf);
        if (frags > 0) {
            s_sent++;
            ts += 90000u / 5u;
        }
        sip_osal_sleep_ms(interval);
    }
    return NULL;
}

int peer_v4l2_start(const char *dev, int want_w, int want_h,
                    const char *remote_ip, uint16_t remote_port,
                    uint16_t rtp_local_port, int fps)
{
    if (s_running) {
        return 0;
    }
    if (!dev || !dev[0]) {
        dev = "/dev/video0";
    }
    if (device_init(dev, want_w, want_h) != 0) {
        device_deinit();
        return -1;
    }
    if (s_w > 240 || s_h > 320) {
        sip_osal_log(SIP_LOG_INFO, _TAG,
                     "camera %dx%d -> letterbox scale to 240x320 before send",
                     s_w, s_h);
    }
    if (sip_rtp_open(&s_rtp, rtp_local_port, SIP_JPEG_PT) != 0) {
        sip_osal_log(SIP_LOG_ERR, _TAG, "rtp bind %u failed", rtp_local_port);
        device_deinit();
        return -1;
    }
    sip_rtp_set_peer(&s_rtp, remote_ip, remote_port);
    (void)fps;

    s_sent = 0;
    s_running = 1;
    pthread_create(&s_thread, NULL, v4l2_thread, NULL);
    sip_osal_log(SIP_LOG_INFO, _TAG, "camera %s %dx%d MJPEG -> %s:%u",
                 dev, s_w, s_h, remote_ip, remote_port);
    return 0;
}

void peer_v4l2_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = 0;
    sip_rtp_close(&s_rtp);
    pthread_join(s_thread, NULL);
    device_deinit();
    sip_osal_log(SIP_LOG_INFO, _TAG, "stopped (sent=%u)", s_sent);
}

int peer_v4l2_running(void)
{
    return s_running;
}

uint32_t peer_v4l2_frames_sent(void)
{
    return s_sent;
}

int peer_v4l2_current_size(int *w, int *h)
{
    if (s_fd < 0) {
        return -1;
    }
    *w = s_w;
    *h = s_h;
    return 0;
}
