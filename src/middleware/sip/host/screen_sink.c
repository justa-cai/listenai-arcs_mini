/*
 * screen_sink.c - 主机侧 LCD 回传接收器（与设备 sip_screen.c 配套）
 *
 *   ./tmp/sip-host/screen_sink [port] [outdir]
 *     port    默认 5070
 *     outdir  默认 tmp/sip_host
 *
 * 按帧 id 重组 16B 头分片协议（见 sip_screen.c 头注释），收齐校验
 * JPEG SOI/EOI 后落盘 screen_NNN.jpg，滚动保留最近 12 张。丢片整帧
 * 作废，等下一帧（设备 1s 一帧）。
 */
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCR_MAGIC        0x5343u
#define SCR_HDR_LEN      16u
#define SCR_BUF_MAX      (256u * 1024u)
#define SCR_BLOCK        256u   /* 位图块粒度 */
#define SCR_BITMAP_BYTES (SCR_BUF_MAX / SCR_BLOCK / 8u)
#define SCR_KEEP_FILES   12u

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t frame_id;
    uint16_t width;
    uint16_t height;
    uint32_t total;
    uint32_t offset;
} scr_hdr_t;

static uint8_t s_buf[SCR_BUF_MAX];
static uint8_t s_bitmap[SCR_BITMAP_BYTES];

static void bitmap_reset(void)
{
    memset(s_bitmap, 0, sizeof(s_bitmap));
}

static void bitmap_set(uint32_t off, uint32_t len)
{
    uint32_t b0 = off / SCR_BLOCK;
    uint32_t b1 = (off + len - 1) / SCR_BLOCK;
    for (uint32_t b = b0; b <= b1 && b < sizeof(s_bitmap) * 8u; ++b) {
        s_bitmap[b / 8u] |= (uint8_t)(1u << (b % 8u));
    }
}

static int bitmap_full(uint32_t total)
{
    uint32_t blocks = (total + SCR_BLOCK - 1u) / SCR_BLOCK;
    for (uint32_t b = 0; b < blocks; ++b) {
        if (!(s_bitmap[b / 8u] & (1u << (b % 8u)))) {
            return 0;
        }
    }
    return 1;
}

static void save_frame(const char *outdir, unsigned seq, const scr_hdr_t *hdr)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/screen_%03u.jpg", outdir, seq);
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[sink] open %s failed: %s\n", path, strerror(errno));
        return;
    }
    fwrite(s_buf, 1, hdr->total, f);
    fclose(f);
    printf("[sink] frame %u saved: %s (%u bytes, %ux%u)\n",
           hdr->frame_id, path, hdr->total, hdr->width, hdr->height);
    if (seq >= SCR_KEEP_FILES) {
        char old[256];
        snprintf(old, sizeof(old), "%s/screen_%03u.jpg", outdir, seq - SCR_KEEP_FILES);
        unlink(old);
    }
}

int main(int argc, char **argv)
{
    uint16_t port = (argc > 1) ? (uint16_t)atoi(argv[1]) : 5070u;
    const char *outdir = (argc > 2) ? argv[2] : "tmp/sip_host";

    mkdir(outdir, 0755); /* 已存在则忽略 */

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        return 1;
    }
    printf("[sink] listening UDP %u -> %s/screen_NNN.jpg\n", port, outdir);

    uint32_t cur_frame = 0xFFFFFFFFu;
    uint32_t cur_total = 0;
    int have_frame = 0;
    unsigned saved = 0;
    uint8_t pkt[2048];

    for (;;) {
        ssize_t n = recvfrom(fd, pkt, sizeof(pkt), 0, NULL, NULL);
        if (n < (ssize_t)SCR_HDR_LEN) {
            continue;
        }
        scr_hdr_t hdr;
        memcpy(&hdr, pkt, sizeof(hdr)); /* 两端同为小端，裸拷 */
        if (hdr.magic != SCR_MAGIC || hdr.total == 0 || hdr.total > SCR_BUF_MAX ||
            hdr.offset >= hdr.total || (uint32_t)n - SCR_HDR_LEN == 0 ||
            hdr.offset + (uint32_t)n - SCR_HDR_LEN > hdr.total) {
            continue;
        }
        if (!have_frame || hdr.frame_id != (uint16_t)cur_frame) {
            have_frame = 1;
            cur_frame = hdr.frame_id;
            cur_total = hdr.total;
            bitmap_reset();
        } else if (hdr.total != cur_total) {
            continue; /* 同帧 id 但 total 变了（回绕撞车），丢 */
        }
        memcpy(s_buf + hdr.offset, pkt + SCR_HDR_LEN, (size_t)n - SCR_HDR_LEN);
        bitmap_set(hdr.offset, (uint32_t)n - SCR_HDR_LEN);
        if (bitmap_full(hdr.total) && s_buf[0] == 0xFF && s_buf[1] == 0xD8 &&
            s_buf[hdr.total - 2] == 0xFF && s_buf[hdr.total - 1] == 0xD9) {
            save_frame(outdir, saved++, &hdr);
            have_frame = 0; /* 收齐，等下一帧 */
        }
    }
}
