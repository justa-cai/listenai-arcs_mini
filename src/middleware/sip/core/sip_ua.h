/*
 * sip_ua.h - SIP 用户代理（点对点 LAN 子集的状态机 + UDP 事务）
 *
 * 单线程 poll 模型：核心不开任务，宿主任务循环调用 sip_ua_poll()。
 * 一次只允许一个对话（单线路电话）。事件通过回调上抛。
 *
 * 状态机：
 *   呼出 IDLE→INVITING→RINGING_OUT→ACTIVE→(BYE)→ENDED→IDLE
 *   呼入 IDLE→RINGING_IN→ACTIVE→(BYE)→ENDED→IDLE
 * 超时：INVITING 32s 无响应失败；RINGING_OUT 60s 发 CANCEL；RINGING_IN 60s 回 486；
 *       ENDED 停留 2s 供 UI 显示，随后自动回 IDLE。
 * 重传：UDP T1=500ms 倍增，INVITE/BYE 最多 7 次；ACK 不重传。
 */
#ifndef SIP_UA_H
#define SIP_UA_H

#include <stdint.h>

#include "sip_msg.h"
#include "sip_osal.h"
#include "sip_sdp.h"

#define SIP_DEFAULT_PORT 5060u

typedef enum {
    SIP_UA_IDLE = 0,
    SIP_UA_INVITING,     /* 已发 INVITE，等 100/180 */
    SIP_UA_RINGING_OUT,  /* 对端振铃（收到 180） */
    SIP_UA_RINGING_IN,   /* 对端呼入，本端待接听 */
    SIP_UA_ACTIVE,       /* 通话中 */
    SIP_UA_ENDED,        /* 已结束，短暂停留后回 IDLE */
} sip_ua_state_t;

/* 协商结果：媒体任务据此建立 RTP 通道。 */
typedef struct {
    char peer_ip[16];
    uint16_t peer_audio_port;
    uint16_t peer_video_port;
    int audio_pt;   /* 0=PCMU 8=PCMA */
    int video;      /* 1=双向视频协商成功 */
} sip_session_t;

typedef void (*sip_ua_state_cb)(void *user, sip_ua_state_t st);
typedef void (*sip_ua_incoming_cb)(void *user, const char *peer_ip, uint16_t sig_port);

typedef struct {
    uint16_t sig_port;    /* 本端信令端口（默认 5060） */
    uint16_t audio_port;  /* 本端音频 RTP 端口（5062） */
    uint16_t video_port;  /* 本端视频 RTP 端口（5064） */
    int offer_video;      /* offer 是否包含视频 */
    const char *local_user; /* URI 用户名（"arcs"/"peer"），可为 NULL */
    sip_ua_state_cb on_state;
    sip_ua_incoming_cb on_incoming;
    void *user;
} sip_ua_cfg_t;

typedef struct sip_ua {
    sip_ua_cfg_t cfg;
    int fd;
    sip_ua_state_t state;
    uint32_t state_ms;

    /* 对话标识（呼出时生成；呼入时从 INVITE 拷贝） */
    char call_id[48];
    char local_tag[16];
    char peer_tag[16];
    char local_ip[16];
    char local_uri[48];
    char peer_ip[16];
    char peer_uri[48];
    uint16_t peer_port;      /* 对端信令端口 */
    int invite_cseq;         /* INVITE 的 CSeq（CANCEL/匹配响应用） */
    char invite_branch[28];  /* 呼入 INVITE 的 branch（487/重构响应用） */
    int cseq_out;            /* 本端请求的 CSeq 计数（INVITE=1，BYE 递增） */
    sip_sdp_t remote_offer;  /* 呼入时收到的 offer（answer 时用） */

    /* 事务重传（同一时刻至多一个待确认请求） */
    char tx_buf[SIP_MSG_MAX];
    int tx_len;
    char tx_method[12];
    int tx_cseq;
    uint32_t tx_next_ms;
    uint32_t tx_interval_ms;
    int tx_left;

    /* UAS 最后一响应（对端重传 INVITE 时重发） */
    char last_resp[SIP_MSG_MAX];
    int last_resp_len;

    sip_session_t session;
} sip_ua_t;

int sip_ua_init(sip_ua_t *ua, const sip_ua_cfg_t *cfg);
void sip_ua_poll(sip_ua_t *ua, uint32_t wait_ms);
int sip_ua_invite(sip_ua_t *ua, const char *peer_ip, uint16_t peer_port);
int sip_ua_answer(sip_ua_t *ua);
int sip_ua_reject(sip_ua_t *ua);
int sip_ua_bye(sip_ua_t *ua);
void sip_ua_cancel(sip_ua_t *ua);
sip_ua_state_t sip_ua_state(const sip_ua_t *ua);
const sip_session_t *sip_ua_session(const sip_ua_t *ua);
uint32_t sip_ua_state_ms(const sip_ua_t *ua);

#endif /* SIP_UA_H */
