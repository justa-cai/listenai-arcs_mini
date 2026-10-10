/*
 * sip_sdp.h - 最小 SDP offer/answer
 *
 * 只解析/生成我们这一对端点用得到的字段：
 *   v= / o= / s= / c=IN IP4 / t= / m=audio|m=video / a=rtpmap
 * 音频编解码 PCMU(0)/PCMA(8)，视频 JPEG(26, RFC2435 静态类型)。
 * m 行端口为 0 表示该媒体被拒/禁用。
 */
#ifndef SIP_SDP_H
#define SIP_SDP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char ip[16];              /* c= 连接地址 */
    int audio_port;           /* 0 = 无音频 / 被拒 */
    int video_port;           /* 0 = 无视频 / 被拒 */
    int has_pcmu;             /* offer 里列出 PT 0 */
    int has_pcma;             /* offer 里列出 PT 8 */
    int has_jpeg;             /* offer 的 m=video 里列出 PT 26 */
} sip_sdp_t;

/* 从 SDP 体解析。返回 0 成功；-1 结构不完整。 */
int sip_sdp_parse(sip_sdp_t *out, const char *body, int len);

/* 生成本端 offer（audio+video 双媒体；with_video=0 时 m=video 端口置 0）。 */
int sip_sdp_build_offer(char *out, int outlen, const char *ip,
                        int audio_port, int video_port, int with_video);

/*
 * 根据 offer 生成本端 answer：
 *   音频选 PCMU（offer 无 PCMU 才选 PCMA），两者皆无返回 -1（上层回 488）；
 *   视频：offer 有 JPEG 则本端也回 JPEG，否则 m=video 0（降级纯音频）。
 */
/* with_video=0 时强制 m=video 0（本端不收/不发视频的降级应答）。 */
int sip_sdp_build_answer(char *out, int outlen, const sip_sdp_t *offer,
                         const char *ip, int audio_port, int video_port,
                         int with_video);

#endif /* SIP_SDP_H */
