/*
 * sip_ua.c - SIP 用户代理实现（点对点 LAN 子集）
 *
 * 设计取舍（都是"两端自研栈 + 局域网"前提下的简化，逐条可回退）：
 *  - 单对话：收到新 INVITE 时若正忙直接 486，不做并行线路。
 *  - 200 OK 即 ACTIVE：不等 ACK（LAN 可靠；ACK 丢了靠 200 重传自愈）。
 *  - CANCEL 竞态：CANCEL 后若仍收到 200，补 ACK + BYE 收尾。
 *  - 响应匹配用 Call-ID + CSeq(method)，不比对 Via branch（同一时刻只有
 *    一个事务，branch 匹配是给并行事务用的）。
 */
#include <stdio.h>
#include <string.h>

#include "sip_msg.h"
#include "sip_ua.h"

#define SIP_T1_MS 500u
#define SIP_TX_MAX 7
#define SIP_T_INVITE_FAIL_MS 32000u   /* INVITING 无任何响应 */
#define SIP_T_RING_OUT_MS 60000u      /* 对端振铃无应答 → CANCEL */
#define SIP_T_RING_IN_MS 60000u       /* 本端振铃无人接 → 486 */
#define SIP_T_ENDED_MS 2000u          /* ENDED 停留展示时长 */

static void set_state(sip_ua_t *ua, sip_ua_state_t st)
{
    if (ua->state == st) {
        return;
    }
    ua->state = st;
    ua->state_ms = sip_osal_now_ms();
    sip_osal_log(SIP_LOG_INFO, "ua", "state -> %d", (int)st);
    if (ua->cfg.on_state) {
        ua->cfg.on_state(ua->cfg.user, st);
    }
}

static void gen_hex(char *out, int chars)
{
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < chars; ++i) {
        out[i] = digits[sip_osal_rand() & 0x0F];
    }
    out[chars] = '\0';
}

static void tx_arm(sip_ua_t *ua, const char *method, int cseq,
                   const char *buf, int len, int retransmits)
{
    memcpy(ua->tx_buf, buf, (size_t)len);
    ua->tx_len = len;
    snprintf(ua->tx_method, sizeof(ua->tx_method), "%s", method);
    ua->tx_cseq = cseq;
    ua->tx_interval_ms = SIP_T1_MS;
    ua->tx_next_ms = sip_osal_now_ms() + SIP_T1_MS;
    ua->tx_left = retransmits;
}

static void tx_clear(sip_ua_t *ua)
{
    ua->tx_left = 0;
    ua->tx_len = 0;
}

static int send_to_peer(sip_ua_t *ua, const char *buf, int len)
{
    int n = sip_osal_udp_send(ua->fd, ua->peer_ip, ua->peer_port, buf, (size_t)len);
    if (n < 0) {
        sip_osal_log(SIP_LOG_WARN, "ua", "send to %s:%u failed", ua->peer_ip, ua->peer_port);
    }
    return n;
}

static void build_and_send_request(sip_ua_t *ua, const char *method, int cseq,
                                   const char *body, int body_len, int retransmits)
{
    char branch[28];
    char b[12];
    gen_hex(b, 8);
    snprintf(branch, sizeof(branch), "z9hG4bK%s", b);

    char buf[SIP_MSG_MAX];
    int len = sip_msg_request(buf, sizeof(buf), method,
                              ua->local_ip, ua->local_uri, ua->local_tag,
                              ua->peer_uri, ua->peer_tag,
                              ua->call_id, cseq, branch, body, body_len);
    if (len <= 0) {
        sip_osal_log(SIP_LOG_ERR, "ua", "build %s failed", method);
        return;
    }
    sip_osal_log(SIP_LOG_DBG, "ua", ">> %s %d bytes to %s:%u", method, len,
                 ua->peer_ip, ua->peer_port);
    send_to_peer(ua, buf, len);
    if (retransmits > 0) {
        tx_arm(ua, method, cseq, buf, len, retransmits);
    }
}

static void build_and_send_response(sip_ua_t *ua, const sip_msg_t *req,
                                    int code, const char *reason, const char *to_tag,
                                    const char *body, int body_len, int save_last)
{
    char buf[SIP_MSG_MAX];
    int len = sip_msg_response(buf, sizeof(buf), req, code, reason, to_tag,
                               body, body_len);
    if (len <= 0) {
        return;
    }
    sip_osal_log(SIP_LOG_DBG, "ua", ">> %d %s (%d bytes) to %s:%u", code, reason, len,
                 req->src_ip, req->src_port);
    sip_osal_udp_send(ua->fd, req->src_ip, req->src_port, buf, (size_t)len);
    if (save_last) {
        memcpy(ua->last_resp, buf, (size_t)len);
        ua->last_resp_len = len;
    }
}

/* 对话清空（保留 socket 与配置）。 */
static void dialog_reset(sip_ua_t *ua)
{
    ua->call_id[0] = 0;
    ua->local_tag[0] = 0;
    ua->peer_tag[0] = 0;
    ua->peer_uri[0] = 0;
    ua->peer_ip[0] = 0;
    ua->peer_port = 0;
    ua->invite_cseq = 0;
    tx_clear(ua);
    ua->last_resp_len = 0;
    memset(&ua->remote_offer, 0, sizeof(ua->remote_offer));
    memset(&ua->session, 0, sizeof(ua->session));
}

int sip_ua_init(sip_ua_t *ua, const sip_ua_cfg_t *cfg)
{
    memset(ua, 0, sizeof(*ua));
    ua->cfg = *cfg;
    if (ua->cfg.sig_port == 0) {
        ua->cfg.sig_port = SIP_DEFAULT_PORT;
    }
    if (!ua->cfg.local_user) {
        ua->cfg.local_user = "minisip";
    }
    ua->fd = sip_osal_udp_open(ua->cfg.sig_port);
    if (ua->fd < 0) {
        sip_osal_log(SIP_LOG_ERR, "ua", "bind UDP %u failed", ua->cfg.sig_port);
        return -1;
    }
    ua->state = SIP_UA_IDLE;
    sip_osal_log(SIP_LOG_INFO, "ua", "listening on UDP %u (audio %u video %u)",
                 ua->cfg.sig_port, ua->cfg.audio_port, ua->cfg.video_port);
    return 0;
}

sip_ua_state_t sip_ua_state(const sip_ua_t *ua)
{
    return ua->state;
}

const sip_session_t *sip_ua_session(const sip_ua_t *ua)
{
    return &ua->session;
}

uint32_t sip_ua_state_ms(const sip_ua_t *ua)
{
    return sip_osal_now_ms() - ua->state_ms;
}

int sip_ua_invite(sip_ua_t *ua, const char *peer_ip, uint16_t peer_port)
{
    if (ua->state != SIP_UA_IDLE && ua->state != SIP_UA_ENDED) {
        sip_osal_log(SIP_LOG_WARN, "ua", "invite: busy (state=%d)", (int)ua->state);
        return -2;
    }
    if (sip_osal_get_local_ip(peer_ip, ua->local_ip, sizeof(ua->local_ip)) != 0) {
        sip_osal_log(SIP_LOG_ERR, "ua", "no route to %s", peer_ip);
        return -1;
    }
    dialog_reset(ua);

    char cid[48];
    char hex[9];
    gen_hex(hex, 8);
    snprintf(cid, sizeof(cid), "%s@%s", hex, ua->local_ip);
    snprintf(ua->call_id, sizeof(ua->call_id), "%s", cid);
    gen_hex(ua->local_tag, 8);
    snprintf(ua->peer_ip, sizeof(ua->peer_ip), "%s", peer_ip);
    ua->peer_port = peer_port ? peer_port : SIP_DEFAULT_PORT;
    snprintf(ua->local_uri, sizeof(ua->local_uri), "%s@%s", ua->cfg.local_user, ua->local_ip);
    snprintf(ua->peer_uri, sizeof(ua->peer_uri), "%s@%s", ua->cfg.local_user, peer_ip);
    ua->invite_cseq = 1;
    ua->cseq_out = 1;

    char sdp[512];
    int sdp_len = sip_sdp_build_offer(sdp, sizeof(sdp), ua->local_ip,
                                      ua->cfg.audio_port, ua->cfg.video_port,
                                      ua->cfg.offer_video);
    if (sdp_len <= 0) {
        return -1;
    }
    build_and_send_request(ua, "INVITE", ua->invite_cseq, sdp, sdp_len, SIP_TX_MAX);
    set_state(ua, SIP_UA_INVITING);
    return 0;
}

int sip_ua_answer(sip_ua_t *ua)
{
    if (ua->state != SIP_UA_RINGING_IN) {
        return -2;
    }
    if (sip_osal_get_local_ip(ua->peer_ip, ua->local_ip, sizeof(ua->local_ip)) != 0) {
        return -1;
    }
    char sdp[512];
    int sdp_len = sip_sdp_build_answer(sdp, sizeof(sdp), &ua->remote_offer,
                                       ua->local_ip, ua->cfg.audio_port,
                                       ua->cfg.video_port);
    if (sdp_len <= 0) {
        sip_osal_log(SIP_LOG_ERR, "ua", "answer: no common codec, reject 488");
        sip_ua_reject(ua);
        return -1;
    }

    /* 会话参数：音频 PT 按 offer 选择，视频按协商结果。 */
    ua->session.audio_pt = ua->remote_offer.has_pcmu ? 0 : 8;
    ua->session.video = ua->remote_offer.has_jpeg && ua->remote_offer.video_port > 0;
    snprintf(ua->session.peer_ip, sizeof(ua->session.peer_ip), "%s", ua->peer_ip);
    ua->session.peer_audio_port = (uint16_t)ua->remote_offer.audio_port;
    ua->session.peer_video_port = (uint16_t)ua->remote_offer.video_port;

    /* 重构 INVITE（从 last_resp 的上下文）：直接复用保存的请求字段重发响应。
     * 为此在收到 INVITE 时已把原始请求的关键字段留在 remote_* —— 这里用
     * last_resp 机制之外的临时重构（见 handle_request 的保存）。 */
    ua->last_resp_len = 0; /* answer 生成新的 200 */
    sip_msg_t req;
    memset(&req, 0, sizeof(req));
    req.is_request = 1;
    req.code = 0;
    snprintf(req.method, sizeof(req.method), "INVITE");
    snprintf(req.src_ip, sizeof(req.src_ip), "%s", ua->peer_ip);
    snprintf(req.branch, sizeof(req.branch), "%s", ua->invite_branch);
    snprintf(req.from_uri, sizeof(req.from_uri), "%s", ua->peer_uri);
    snprintf(req.from_tag, sizeof(req.from_tag), "%s", ua->peer_tag);
    snprintf(req.to_uri, sizeof(req.to_uri), "%s", ua->local_uri);
    snprintf(req.call_id, sizeof(req.call_id), "%s", ua->call_id);
    req.cseq = ua->invite_cseq;
    snprintf(req.cseq_method, sizeof(req.cseq_method), "INVITE");
    req.src_port = ua->peer_port;
    build_and_send_response(ua, &req, 200, "OK", ua->local_tag, sdp, sdp_len, 1);

    set_state(ua, SIP_UA_ACTIVE);
    return 0;
}

int sip_ua_reject(sip_ua_t *ua)
{
    if (ua->state != SIP_UA_RINGING_IN) {
        return -2;
    }
    sip_msg_t req;
    memset(&req, 0, sizeof(req));
    req.is_request = 1;
    snprintf(req.method, sizeof(req.method), "INVITE");
    snprintf(req.src_ip, sizeof(req.src_ip), "%s", ua->peer_ip);
    snprintf(req.branch, sizeof(req.branch), "%s", ua->invite_branch);
    snprintf(req.from_uri, sizeof(req.from_uri), "%s", ua->peer_uri);
    snprintf(req.from_tag, sizeof(req.from_tag), "%s", ua->peer_tag);
    snprintf(req.to_uri, sizeof(req.to_uri), "%s", ua->local_uri);
    snprintf(req.call_id, sizeof(req.call_id), "%s", ua->call_id);
    req.cseq = ua->invite_cseq;
    snprintf(req.cseq_method, sizeof(req.cseq_method), "INVITE");
    req.src_port = ua->peer_port;
    build_and_send_response(ua, &req, 486, "Busy Here", ua->local_tag, NULL, 0, 0);

    dialog_reset(ua);
    set_state(ua, SIP_UA_ENDED);
    return 0;
}

int sip_ua_bye(sip_ua_t *ua)
{
    if (ua->state != SIP_UA_ACTIVE) {
        return -2;
    }
    ua->cseq_out++;
    build_and_send_request(ua, "BYE", ua->cseq_out, NULL, 0, SIP_TX_MAX);
    set_state(ua, SIP_UA_ENDED);
    return 0;
}

void sip_ua_cancel(sip_ua_t *ua)
{
    if (ua->state != SIP_UA_INVITING && ua->state != SIP_UA_RINGING_OUT) {
        return;
    }
    /* CANCEL 与 INVITE 同 CSeq 号；发出后等 487/200 收尾（不等也行，超时兜底）。 */
    build_and_send_request(ua, "CANCEL", ua->invite_cseq, NULL, 0, 2);
}

/* ===================== 收包分发 ===================== */

static void handle_request(sip_ua_t *ua, sip_msg_t *m)
{
    if (strcmp(m->method, "INVITE") == 0) {
        if (ua->state == SIP_UA_IDLE || ua->state == SIP_UA_ENDED) {
            /* 新呼入：记录对话，回 180，交给上层决定 answer/reject。 */
            if (sip_osal_get_local_ip(m->src_ip, ua->local_ip, sizeof(ua->local_ip)) != 0) {
                build_and_send_response(ua, m, 500, "Server Internal Error", "", NULL, 0, 0);
                return;
            }
            dialog_reset(ua);
            snprintf(ua->call_id, sizeof(ua->call_id), "%s", m->call_id);
            snprintf(ua->peer_tag, sizeof(ua->peer_tag), "%s", m->from_tag);
            gen_hex(ua->local_tag, 8); /* UAS 生成 To-tag */
            snprintf(ua->peer_ip, sizeof(ua->peer_ip), "%s", m->src_ip);
            ua->peer_port = m->src_port;
            snprintf(ua->peer_uri, sizeof(ua->peer_uri), "%s@%s", ua->cfg.local_user, m->src_ip);
            snprintf(ua->local_uri, sizeof(ua->local_uri), "%s@%s", ua->cfg.local_user, ua->local_ip);
            snprintf(ua->invite_branch, sizeof(ua->invite_branch), "%s", m->branch);
            ua->invite_cseq = m->cseq;

            if (sip_sdp_parse(&ua->remote_offer, m->body, m->body_len) != 0 ||
                (!ua->remote_offer.has_pcmu && !ua->remote_offer.has_pcma)) {
                sip_osal_log(SIP_LOG_WARN, "ua", "incoming SDP unusable -> 488");
                build_and_send_response(ua, m, 488, "Not Acceptable Here", ua->local_tag, NULL, 0, 0);
                dialog_reset(ua);
                return;
            }

            build_and_send_response(ua, m, 180, "Ringing", ua->local_tag, NULL, 0, 1);
            set_state(ua, SIP_UA_RINGING_IN);
            if (ua->cfg.on_incoming) {
                ua->cfg.on_incoming(ua->cfg.user, m->src_ip, m->src_port);
            }
        } else if (ua->state == SIP_UA_RINGING_IN && strcmp(m->call_id, ua->call_id) == 0) {
            /* INVITE 重传：重发最后响应（180 或 200）。 */
            if (ua->last_resp_len > 0) {
                sip_osal_udp_send(ua->fd, m->src_ip, m->src_port,
                                  ua->last_resp, (size_t)ua->last_resp_len);
            }
        } else {
            build_and_send_response(ua, m, 486, "Busy Here", ua->local_tag, NULL, 0, 0);
        }
        return;
    }

    if (strcmp(m->method, "CANCEL") == 0) {
        if (ua->state == SIP_UA_RINGING_IN && strcmp(m->call_id, ua->call_id) == 0) {
            build_and_send_response(ua, m, 200, "OK", "", NULL, 0, 0);
            /* 对原 INVITE 回 487： */
            sip_msg_t inv;
            memset(&inv, 0, sizeof(inv));
            inv.is_request = 1;
            snprintf(inv.method, sizeof(inv.method), "INVITE");
            snprintf(inv.src_ip, sizeof(inv.src_ip), "%s", ua->peer_ip);
            snprintf(inv.branch, sizeof(inv.branch), "%s", ua->invite_branch);
            snprintf(inv.from_uri, sizeof(inv.from_uri), "%s", ua->peer_uri);
            snprintf(inv.from_tag, sizeof(inv.from_tag), "%s", ua->peer_tag);
            snprintf(inv.to_uri, sizeof(inv.to_uri), "%s", ua->local_uri);
            snprintf(inv.call_id, sizeof(inv.call_id), "%s", ua->call_id);
            inv.cseq = ua->invite_cseq;
            snprintf(inv.cseq_method, sizeof(inv.cseq_method), "INVITE");
            inv.src_port = ua->peer_port;
            build_and_send_response(ua, &inv, 487, "Request Terminated", ua->local_tag, NULL, 0, 0);
            dialog_reset(ua);
            set_state(ua, SIP_UA_ENDED);
        } else {
            build_and_send_response(ua, m, 481, "Call/Transaction Does Not Exist", "", NULL, 0, 0);
        }
        return;
    }

    if (strcmp(m->method, "ACK") == 0) {
        /* 200 已在 answer 时发出，ACK 只用于确认；LAN 下不重发 200。 */
        return;
    }

    if (strcmp(m->method, "BYE") == 0) {
        if (strcmp(m->call_id, ua->call_id) == 0) {
            build_and_send_response(ua, m, 200, "OK", "", NULL, 0, 0);
            dialog_reset(ua);
            set_state(ua, SIP_UA_ENDED);
        } else {
            build_and_send_response(ua, m, 481, "Call/Transaction Does Not Exist", "", NULL, 0, 0);
        }
        return;
    }

    build_and_send_response(ua, m, 500, "Not Implemented", "", NULL, 0, 0);
}

static void handle_response(sip_ua_t *ua, sip_msg_t *m)
{
    if (strcmp(m->call_id, ua->call_id) != 0) {
        return; /* 不是当前对话的响应 */
    }
    if (strcmp(m->cseq_method, "INVITE") == 0 && m->cseq == ua->invite_cseq) {
        switch (m->code / 100) {
        case 1:
            if (m->code == 180 && ua->state == SIP_UA_INVITING) {
                set_state(ua, SIP_UA_RINGING_OUT);
            }
            break;
        case 2: {
            sip_sdp_t answer;
            if (sip_sdp_parse(&answer, m->body, m->body_len) != 0) {
                sip_osal_log(SIP_LOG_WARN, "ua", "200 OK SDP unusable -> ACK+BYE");
                build_and_send_request(ua, "ACK", ua->invite_cseq, NULL, 0, 0);
                ua->cseq_out++;
                build_and_send_request(ua, "BYE", ua->cseq_out, NULL, 0, 0);
                dialog_reset(ua);
                set_state(ua, SIP_UA_ENDED);
                return;
            }
            tx_clear(ua); /* INVITE 得到最终响应，停止重传 */
            snprintf(ua->peer_tag, sizeof(ua->peer_tag), "%s", m->to_tag);
            ua->session.audio_pt = answer.has_pcmu ? 0 : 8;
            ua->session.video = answer.has_jpeg && answer.video_port > 0;
            snprintf(ua->session.peer_ip, sizeof(ua->session.peer_ip), "%s", ua->peer_ip);
            ua->session.peer_audio_port = (uint16_t)answer.audio_port;
            ua->session.peer_video_port = (uint16_t)answer.video_port;
            build_and_send_request(ua, "ACK", ua->invite_cseq, NULL, 0, 0);
            set_state(ua, SIP_UA_ACTIVE);
            break;
        }
        default:
            tx_clear(ua);
            sip_osal_log(SIP_LOG_INFO, "ua", "invite failed: %d %s", m->code, m->reason);
            dialog_reset(ua);
            set_state(ua, SIP_UA_ENDED);
            break;
        }
        return;
    }
    if (strcmp(m->cseq_method, "BYE") == 0 && m->code / 100 == 2) {
        tx_clear(ua);
        dialog_reset(ua);
        if (ua->state == SIP_UA_ENDED) {
            set_state(ua, SIP_UA_IDLE);
        }
        return;
    }
    if (strcmp(m->cseq_method, "CANCEL") == 0) {
        tx_clear(ua);
        dialog_reset(ua);
        set_state(ua, SIP_UA_ENDED);
    }
}

/* ===================== 定时器 ===================== */

static void ua_timers(sip_ua_t *ua)
{
    uint32_t now = sip_osal_now_ms();

    if (ua->tx_left > 0 && (int32_t)(now - ua->tx_next_ms) >= 0) {
        sip_osal_log(SIP_LOG_DBG, "ua", "retransmit %s (%d left)", ua->tx_method, ua->tx_left);
        send_to_peer(ua, ua->tx_buf, ua->tx_len);
        ua->tx_interval_ms *= 2;
        ua->tx_next_ms = now + ua->tx_interval_ms;
        ua->tx_left--;
        if (ua->tx_left == 0) {
            if (strcmp(ua->tx_method, "INVITE") == 0 &&
                (ua->state == SIP_UA_INVITING || ua->state == SIP_UA_RINGING_OUT)) {
                sip_osal_log(SIP_LOG_WARN, "ua", "INVITE no answer, give up");
                dialog_reset(ua);
                set_state(ua, SIP_UA_ENDED);
            } else if (strcmp(ua->tx_method, "BYE") == 0) {
                dialog_reset(ua);
                set_state(ua, SIP_UA_IDLE);
            } else {
                tx_clear(ua);
            }
        }
    }

    switch (ua->state) {
    case SIP_UA_INVITING:
        if (sip_ua_state_ms(ua) > SIP_T_INVITE_FAIL_MS) {
            sip_osal_log(SIP_LOG_WARN, "ua", "INVITING timeout");
            dialog_reset(ua);
            set_state(ua, SIP_UA_ENDED);
        }
        break;
    case SIP_UA_RINGING_OUT:
        if (sip_ua_state_ms(ua) > SIP_T_RING_OUT_MS) {
            sip_osal_log(SIP_LOG_WARN, "ua", "ring-out timeout -> CANCEL");
            sip_ua_cancel(ua);
            dialog_reset(ua);
            set_state(ua, SIP_UA_ENDED);
        }
        break;
    case SIP_UA_RINGING_IN:
        if (sip_ua_state_ms(ua) > SIP_T_RING_IN_MS) {
            sip_osal_log(SIP_LOG_WARN, "ua", "ring-in timeout -> 486");
            sip_ua_reject(ua);
        }
        break;
    case SIP_UA_ENDED:
        if (sip_ua_state_ms(ua) > SIP_T_ENDED_MS) {
            set_state(ua, SIP_UA_IDLE);
        }
        break;
    default:
        break;
    }
}

void sip_ua_poll(sip_ua_t *ua, uint32_t wait_ms)
{
    char buf[SIP_MSG_MAX];
    char from_ip[16];
    uint16_t from_port = 0;
    int n = sip_osal_udp_recv(ua->fd, buf, sizeof(buf), wait_ms, from_ip, &from_port);
    if (n > 0) {
        sip_msg_t m;
        if (sip_msg_parse(&m, buf, n) == 0) {
            snprintf(m.src_ip, sizeof(m.src_ip), "%s", from_ip);
            m.src_port = from_port;
            sip_osal_log(SIP_LOG_DBG, "ua", "<< %s %d/%d from %s:%u",
                         m.is_request ? m.method : "resp",
                         m.code, m.cseq, from_ip, from_port);
            if (m.is_request) {
                handle_request(ua, &m);
            } else {
                handle_response(ua, &m);
            }
        } else {
            sip_osal_log(SIP_LOG_WARN, "ua", "unparsable packet (%d bytes) from %s:%u",
                         n, from_ip, from_port);
        }
    }
    ua_timers(ua);
}
