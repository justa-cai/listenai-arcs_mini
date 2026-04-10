#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lwip/ip_addr.h"
#include "lisa_log.h"
#include "cJSON.h"

#include "httpdns.h"

#define TAG "httpdns_aliyun"

#define HTTPDNS_SERVER      "203.107.1.1"
#define HTTPDNS_PORT        80
#define HTTPDNS_PATH_FMT    "/%s/d?host=%s"

const char *httpdns_get_server(void)
{
    return HTTPDNS_SERVER;
}

int httpdns_resolve(const char *host, ip_addr_t *addr)
{
    char *path = NULL;
    char *rsp_buf = NULL;
    int rsp_len = 0;
    int ret = -1;
    uint32_t ttl = DNS_CACHE_TTL_SEC;

    if (httpdns_cache_lookup(host, addr) == 0) {
        return 0;
    }

    path = malloc(HTTPDNS_REQ_MAXLEN);
    rsp_buf = malloc(HTTPDNS_RSP_MAXLEN);
    if (!path || !rsp_buf) {
        LOGE("httpdns malloc failed");
        goto out;
    }

    snprintf(path, HTTPDNS_REQ_MAXLEN, HTTPDNS_PATH_FMT, CONFIG_HTTPDNS_ALIYUN_ACCOUNT_ID, host);

    LOGI("httpdns request: %s%s", HTTPDNS_SERVER, path);

    ret = httpdns_http_get(HTTPDNS_SERVER, HTTPDNS_PORT, path, rsp_buf, HTTPDNS_RSP_MAXLEN, &rsp_len);
    if (ret != 0 || rsp_len <= 0) {
        LOGE("httpdns http request failed");
        ret = -1;
        goto out;
    }

    char *body = httpdns_http_get_body(rsp_buf);
    if (!body || *body == '\0') {
        LOGE("httpdns empty body");
        ret = -1;
        goto out;
    }

    LOGI("httpdns response body: %s", body);

    /* Aliyun returns JSON: {"host":"xxx","ips":["1.2.3.4","5.6.7.8"],"ttl":600,...} */
    {
        cJSON *root = cJSON_Parse(body);
        if (!root) {
            LOGE("httpdns json parse failed");
            ret = -1;
            goto out;
        }

        cJSON *ips = cJSON_GetObjectItem(root, "ips");
        if (!ips || !cJSON_IsArray(ips) || cJSON_GetArraySize(ips) == 0) {
            LOGE("httpdns no ips in response");
            cJSON_Delete(root);
            ret = -1;
            goto out;
        }

        cJSON *first_ip = cJSON_GetArrayItem(ips, 0);
        if (!first_ip || !cJSON_IsString(first_ip)) {
            LOGE("httpdns invalid ip format");
            cJSON_Delete(root);
            ret = -1;
            goto out;
        }

        cJSON *ttl_json = cJSON_GetObjectItem(root, "ttl");
        if (ttl_json && cJSON_IsNumber(ttl_json)) {
            ttl = (uint32_t)ttl_json->valueint;
        }

        if (!ipaddr_aton(first_ip->valuestring, addr)) {
            LOGE("httpdns invalid ip: %s", first_ip->valuestring);
            cJSON_Delete(root);
            ret = -1;
            goto out;
        }

        cJSON_Delete(root);
    }

    httpdns_cache_add(host, addr, ttl);

    LOGI("httpdns resolved %s -> %s (ttl=%u)", host, ipaddr_ntoa(addr), ttl);
    ret = 0;

out:
    free(path);
    free(rsp_buf);
    return ret;
}
