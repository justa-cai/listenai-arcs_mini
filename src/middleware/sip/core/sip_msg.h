/*
 * sip_msg.h - SIP 消息模型 + 解析/构造
 *
 * 只支持本 SDK 的子集：请求 INVITE/ACK/BYE/CANCEL，响应 100/180/200/481/486/487/488/500。
 * 头部只取对话与事务必需的字段；其余（Route/Record-Route/Supported 等）P2P LAN
 * 场景不需要，直接忽略。
 */
#ifndef SIP_MSG_H
#define SIP_MSG_H

#include <stddef.h>
#include <stdint.h>

#define SIP_MSG_MAX 2048

typedef struct {
    int is_request;          /* 1 = 请求，0 = 响应 */
    char method[12];         /* 请求方法；响应里为空 */
    int code;                /* 响应码；请求里为 0 */
    char reason[24];

    char branch[28];         /* Via 的 branch 参数值（含 z9hG4bK 前缀） */
    char from_uri[80];       /* 头里的 URI 部分（去 <> 与 ;tag=），响应时回显 */
    char from_tag[16];
    char to_uri[80];
    char to_tag[16];         /* 可能空（初始 INVITE 的 To 无 tag） */
    char call_id[48];
    int cseq;
    char cseq_method[12];

    char body[SIP_MSG_MAX / 2];
    int body_len;

    char src_ip[16];         /* 由调用方（UA）在收包时填写 */
    uint16_t src_port;
} sip_msg_t;

/* 解析一条收到的报文。返回 0 成功；-1 格式不认识（丢弃）。 */
int sip_msg_parse(sip_msg_t *m, const char *buf, int len);

/* 构造请求。peer_tag 可为空串（初始 INVITE）。body 为 SDP 或 NULL。 */
int sip_msg_request(char *out, int outlen, const char *method,
                    const char *local_ip, const char *local_uri, const char *local_tag,
                    const char *peer_uri, const char *peer_tag,
                    const char *call_id, int cseq, const char *branch,
                    const char *body, int body_len);

/* 构造响应：回显请求的 Via/From/To/Call-ID/CSeq，按需补 To-tag 与 SDP。 */
int sip_msg_response(char *out, int outlen, const sip_msg_t *req,
                     int code, const char *reason, const char *to_tag,
                     const char *body, int body_len);

#endif /* SIP_MSG_H */
