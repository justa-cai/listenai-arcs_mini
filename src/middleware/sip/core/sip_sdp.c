/*
 * sip_sdp.c - 最小 SDP 解析/生成
 *
 * 逐行扫描，遇到 m= 行时解析端口与整条 payload 列表（静态 PT 可以不写
 * rtpmap，所以以 m= 行的格式列表为准，rtpmap 行只作补充确认）。
 * 忽略所有不认识的行（a=sendrecv 等对 P2P 语义无影响）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sip_sdp.h"

/* 解析一条 m= 行：m=<media> <port> <proto> <pt> ...，记录端口与 PT 列表。 */
static void parse_m_line(const char *line, int *port, int *pts, int pts_max, int *pt_count)
{
    char work[160];
    snprintf(work, sizeof(work), "%s", line + 2); /* 跳过 "m=" */
    char *save = NULL;
    char *media = strtok_r(work, " ", &save);
    char *port_s = strtok_r(NULL, " ", &save);
    char *proto = strtok_r(NULL, " ", &save);
    (void)media;
    (void)proto;
    if (!port_s) {
        return;
    }
    *port = atoi(port_s);
    *pt_count = 0;
    char *tok;
    while ((tok = strtok_r(NULL, " ", &save)) != NULL && *pt_count < pts_max) {
        pts[(*pt_count)++] = atoi(tok);
    }
}

int sip_sdp_parse(sip_sdp_t *out, const char *body, int len)
{
    if (!out || !body || len <= 0) {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    char work[1024];
    size_t avail = (size_t)len;
    if (avail >= sizeof(work)) {
        avail = sizeof(work) - 1;
    }
    memcpy(work, body, avail);
    work[avail] = '\0';

    int has_v = 0;
    char *save = NULL;
    for (char *line = strtok_r(work, "\r\n", &save); line;
         line = strtok_r(NULL, "\r\n", &save)) {
        if (strncmp(line, "v=", 2) == 0) {
            has_v = 1;
        } else if (strncmp(line, "c=IN IP4 ", 9) == 0) {
            snprintf(out->ip, sizeof(out->ip), "%s", line + 9);
        } else if (strncmp(line, "m=audio ", 8) == 0) {
            int pts[8];
            int pt_count = 0;
            parse_m_line(line, &out->audio_port, pts, 8, &pt_count);
            for (int i = 0; i < pt_count; ++i) {
                if (pts[i] == 0) {
                    out->has_pcmu = 1;
                } else if (pts[i] == 8) {
                    out->has_pcma = 1;
                }
            }
        } else if (strncmp(line, "m=video ", 8) == 0) {
            int pts[8];
            int pt_count = 0;
            parse_m_line(line, &out->video_port, pts, 8, &pt_count);
            for (int i = 0; i < pt_count; ++i) {
                if (pts[i] == 26) {
                    out->has_jpeg = 1;
                }
            }
        } else if (strncmp(line, "a=rtpmap:", 9) == 0) {
            /* 补充确认：仅在找不到对应 m= 行时才有意义，正常不触发。 */
            int pt = atoi(line + 9);
            if (pt == 26 && out->video_port > 0) {
                out->has_jpeg = 1;
            }
        }
    }

    if (!has_v || !out->ip[0]) {
        return -1;
    }
    return 0;
}

int sip_sdp_build_offer(char *out, int outlen, const char *ip,
                        int audio_port, int video_port, int with_video)
{
    int n = snprintf(out, outlen,
                     "v=0\r\n"
                     "o=- minisip 1 IN IP4 %s\r\n"
                     "s=sipcall\r\n"
                     "c=IN IP4 %s\r\n"
                     "t=0 0\r\n"
                     "m=audio %d RTP/AVP 0 8\r\n"
                     "a=rtpmap:0 PCMU/8000\r\n"
                     "a=rtpmap:8 PCMA/8000\r\n"
                     "a=ptime:20\r\n"
                     "a=sendrecv\r\n"
                     "m=video %d RTP/AVP 26\r\n"
                     "a=rtpmap:26 JPEG/90000\r\n"
                     "a=framerate:5\r\n",
                     ip, ip, audio_port, with_video ? video_port : 0);
    return (n < 0 || n >= outlen) ? -1 : n;
}

int sip_sdp_build_answer(char *out, int outlen, const sip_sdp_t *offer,
                         const char *ip, int audio_port, int video_port)
{
    if (!offer->has_pcmu && !offer->has_pcma) {
        return -1; /* 无可用音频编解码 → 上层回 488 */
    }
    int pt = offer->has_pcmu ? 0 : 8;
    int video = (offer->has_jpeg && offer->video_port > 0);

    int n = snprintf(out, outlen,
                     "v=0\r\n"
                     "o=- minisip 1 IN IP4 %s\r\n"
                     "s=sipcall\r\n"
                     "c=IN IP4 %s\r\n"
                     "t=0 0\r\n"
                     "m=audio %d RTP/AVP %d\r\n"
                     "a=rtpmap:%d %s/8000\r\n"
                     "a=ptime:20\r\n"
                     "a=sendrecv\r\n"
                     "m=video %d RTP/AVP 26\r\n",
                     ip, ip, audio_port, pt, pt,
                     pt == 0 ? "PCMU" : "PCMA",
                     video ? video_port : 0);
    return (n < 0 || n >= outlen) ? -1 : n;
}
