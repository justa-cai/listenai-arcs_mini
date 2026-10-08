/*
 * sip_msg.c - SIP 消息解析/构造实现
 *
 * 解析按行扫描（请求/状态行 + 头 + 空行 + 体），名字匹配不区分大小写。
 * 报文来自 UDP 单个数据报，天然有边界，Content-Length 只用于校验。
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sip_msg.h"

/* 把一行里 "name:" 后的值拷出来（去首尾空白），未找到返回 0。 */
static int header_value(const char *line, const char *name, char *out, size_t outsz)
{
    size_t nlen = strlen(name);
    if (strncasecmp(line, name, nlen) != 0 || line[nlen] != ':') {
        return 0;
    }
    const char *v = line + nlen + 1;
    while (*v == ' ' || *v == '\t') {
        v++;
    }
    size_t i = 0;
    while (v[i] && v[i] != '\r' && v[i] != '\n' && i + 1 < outsz) {
        out[i] = v[i];
        i++;
    }
    while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) {
        i--;
    }
    out[i] = '\0';
    return (int)i;
}

/* 在头值里找 ";key=..." 参数并拷出。 */
static int param_value(const char *value, const char *key, char *out, size_t outsz)
{
    size_t klen = strlen(key);
    const char *p = value;
    while ((p = strchr(p, ';')) != NULL) {
        p++;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            size_t i = 0;
            while (v[i] && v[i] != ';' && v[i] != '\r' && v[i] != '\n' && i + 1 < outsz) {
                out[i] = v[i];
                i++;
            }
            out[i] = '\0';
            return (int)i;
        }
    }
    return 0;
}

/* 从 From/To 头值里取 URI：去掉 <>、去掉 ;tag= 之后的部分。 */
static void uri_from_header(const char *value, char *out, size_t outsz)
{
    const char *s = value;
    if (*s == '<') {
        s++;
    }
    size_t i = 0;
    while (s[i] && s[i] != '>' && s[i] != ';' && s[i] != '\r' && s[i] != '\n' &&
           i + 1 < outsz) {
        out[i] = s[i];
        i++;
    }
    while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) {
        i--;
    }
    out[i] = '\0';
}

/* 定长拷贝（截断是设计行为，不告警）：头字段提取统一走这里。 */
static void scopy(char *dst, size_t dstsz, const char *src)
{
    size_t i = 0;
    while (src && src[i] && i + 1 < dstsz) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int sip_msg_parse(sip_msg_t *m, const char *buf, int len)
{
    if (!m || !buf || len < 16 || len > SIP_MSG_MAX) {
        return -1;
    }
    memset(m, 0, sizeof(*m));

    /* 只在报文内部扫描，保证不越界读。 */
    char work[SIP_MSG_MAX + 1];
    memcpy(work, buf, (size_t)len);
    work[len] = '\0';

    /* 起始行 */
    const char *eol = strstr(work, "\r\n");
    if (!eol) {
        return -1;
    }
    char firstline[128];
    size_t flen = (size_t)(eol - work);
    if (flen >= sizeof(firstline)) {
        return -1;
    }
    memcpy(firstline, work, flen);
    firstline[flen] = '\0';

    if (strncmp(firstline, "SIP/2.0 ", 8) == 0) {
        char reason[32] = {0};
        if (sscanf(firstline, "SIP/2.0 %d %31[^\r\n]", &m->code, reason) < 1) {
            return -1;
        }
        m->is_request = 0;
        scopy(m->reason, sizeof(m->reason), reason);
    } else {
        char uri[96];
        char ver[16];
        if (sscanf(firstline, "%11s %95s %15s", m->method, uri, ver) != 3 ||
            strncmp(ver, "SIP/2.0", 7) != 0) {
            return -1;
        }
        m->is_request = 1;
        m->code = 0;
    }

    /* 头部逐行 */
    const char *cur = eol + 2;
    int content_length = -1;
    char value[SIP_MSG_MAX / 4];
    while (cur && *cur && *cur != '\r') {
        const char *next = strstr(cur, "\r\n");
        size_t ll = next ? (size_t)(next - cur) : strlen(cur);
        char line[SIP_MSG_MAX / 2];
        if (ll >= sizeof(line)) {
            ll = sizeof(line) - 1;
        }
        memcpy(line, cur, ll);
        line[ll] = '\0';

        if (header_value(line, "Via", value, sizeof(value)) ||
            header_value(line, "v", value, sizeof(value))) {
            param_value(value, "branch", m->branch, sizeof(m->branch));
        } else if (header_value(line, "From", value, sizeof(value)) ||
                   header_value(line, "f", value, sizeof(value))) {
            uri_from_header(value, m->from_uri, sizeof(m->from_uri));
            param_value(value, "tag", m->from_tag, sizeof(m->from_tag));
        } else if (header_value(line, "To", value, sizeof(value)) ||
                   header_value(line, "t", value, sizeof(value))) {
            uri_from_header(value, m->to_uri, sizeof(m->to_uri));
            param_value(value, "tag", m->to_tag, sizeof(m->to_tag));
        } else if (header_value(line, "Call-ID", value, sizeof(value)) ||
                   header_value(line, "i", value, sizeof(value))) {
            scopy(m->call_id, sizeof(m->call_id), value);
        } else if (header_value(line, "CSeq", value, sizeof(value))) {
            char method[12];
            if (sscanf(value, "%d %11s", &m->cseq, method) == 2) {
                snprintf(m->cseq_method, sizeof(m->cseq_method), "%s", method);
            }
        } else if (header_value(line, "Content-Length", value, sizeof(value)) ||
                   header_value(line, "l", value, sizeof(value))) {
            content_length = atoi(value);
        }

        cur = next ? next + 2 : NULL;
    }

    /* 体：空行之后 */
    const char *body = NULL;
    if (cur && *cur == '\r' && cur[1] == '\n') {
        body = cur + 2;
    }
    if (body && *body) {
        size_t avail = (size_t)len - (size_t)(body - work);
        if (content_length >= 0 && (size_t)content_length < avail) {
            avail = (size_t)content_length;
        }
        if (avail > sizeof(m->body) - 1) {
            avail = sizeof(m->body) - 1;
        }
        memcpy(m->body, body, avail);
        m->body[avail] = '\0';
        m->body_len = (int)avail;
    }

    if (!m->call_id[0] || !m->branch[0] || !m->cseq_method[0]) {
        return -1; /* 缺对话/事务必需字段 */
    }
    return 0;
}

int sip_msg_request(char *out, int outlen, const char *method,
                    const char *local_ip, const char *local_uri, const char *local_tag,
                    const char *peer_uri, const char *peer_tag,
                    const char *call_id, int cseq, const char *branch,
                    const char *body, int body_len)
{
    int n = snprintf(out, outlen,
                     "%s %s SIP/2.0\r\n"
                     "Via: SIP/2.0/UDP %s;branch=%s\r\n"
                     "Max-Forwards: 70\r\n"
                     "From: <sip:%s>;tag=%s\r\n"
                     "To: <sip:%s>%s%s\r\n"
                     "Call-ID: %s\r\n"
                     "CSeq: %d %s\r\n"
                     "Contact: <sip:%s>\r\n",
                     method, peer_uri, local_ip, branch,
                     local_uri, local_tag, peer_uri,
                     peer_tag[0] ? ";tag=" : "", peer_tag,
                     call_id, cseq, method, local_uri);
    if (n < 0 || n >= outlen) {
        return -1;
    }
    if (body && body_len > 0) {
        int m2 = snprintf(out + n, outlen - n,
                          "Content-Type: application/sdp\r\n"
                          "Content-Length: %d\r\n\r\n",
                          body_len);
        if (m2 < 0 || n + m2 >= outlen) {
            return -1;
        }
        n += m2;
        if (body_len + n > outlen) {
            return -1;
        }
        memcpy(out + n, body, (size_t)body_len);
        n += body_len;
        out[n] = '\0';
    } else {
        int m2 = snprintf(out + n, outlen - n, "Content-Length: 0\r\n\r\n");
        if (m2 < 0 || n + m2 >= outlen) {
            return -1;
        }
        n += m2;
    }
    return n;
}

int sip_msg_response(char *out, int outlen, const sip_msg_t *req,
                     int code, const char *reason, const char *to_tag,
                     const char *body, int body_len)
{
    /* Via/From/To/Call-ID/CSeq 全部回显请求内容（RFC 要求；也是唯一正确做法），
     * 仅在 To 上按需补 tag（UAS 应答 INVITE 时生成）。 */
    int n = snprintf(out, outlen,
                     "SIP/2.0 %d %s\r\n"
                     "Via: SIP/2.0/UDP %s;branch=%s\r\n"
                     "From: <sip:%s>;tag=%s\r\n"
                     "To: <sip:%s>%s%s\r\n"
                     "Call-ID: %s\r\n"
                     "CSeq: %d %s\r\n",
                     code, reason, req->src_ip, req->branch,
                     req->from_uri[0] ? req->from_uri : req->src_ip, req->from_tag,
                     req->to_uri[0] ? req->to_uri : req->src_ip,
                     to_tag[0] ? ";tag=" : "", to_tag,
                     req->call_id, req->cseq, req->cseq_method);
    if (n < 0 || n >= outlen) {
        return -1;
    }
    if (body && body_len > 0) {
        int m2 = snprintf(out + n, outlen - n,
                          "Content-Type: application/sdp\r\n"
                          "Content-Length: %d\r\n\r\n",
                          body_len);
        if (m2 < 0 || n + m2 >= outlen || body_len + n + m2 > outlen) {
            return -1;
        }
        memcpy(out + n + m2, body, (size_t)body_len);
        n += m2 + body_len;
        out[n] = '\0';
    } else {
        int m2 = snprintf(out + n, outlen - n, "Content-Length: 0\r\n\r\n");
        if (m2 < 0 || n + m2 >= outlen) {
            return -1;
        }
        n += m2;
    }
    return n;
}
