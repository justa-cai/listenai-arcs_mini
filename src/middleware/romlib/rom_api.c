#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "rom_api.h"

#include "cJSON.h"
#include "gamepad.h"
#include "kv_user.h"
#include "lisa_http.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "lisa_mem.h"

#define TAG "rom_api"

#define ROM_API_DEFAULT_PORT       38202
#define ROM_API_REQUEST_TIMEOUT_S  5
/* 下载可能到 1MB, 给足到 30s (弱网下 1MB 要几秒) */
#define ROM_API_DOWNLOAD_TIMEOUT_S 30

/* lisa_http 实例内 url 缓冲为 1024B, 但底层 HTTP URI 缓冲更小, 实测偏长的 URL
 * 会被截断成错误请求。这里按经验值收敛, 超长直接报参数错而不是发出错误请求。 */
#define ROM_API_URL_MAX  480

/* 路径 URL 编码后的上限: 底层 URI 缓冲 512B, 还要留出 base 与前缀/后缀 */
#define ROM_API_PATH_ENC_MAX 400

/* ------------------------------------------------------------------ */
/* 地址推导                                                             */
/* ------------------------------------------------------------------ */

/* 归一化用户填写的地址: 去空白 / 去尾 '/', 缺 http:// 补前缀, 缺端口补默认端口。
 * 只支持明文 http (设备侧 HTTP 客户端不支持 TLS)。 */
static int normalize_base(const char *raw, char *buf, size_t len)
{
    const char *host = raw;
    size_t host_len;
    char tmp[ROM_API_URL_MAX];

    while (*host == ' ' || *host == '\t') {
        host++;
    }
    if (strncmp(host, "http://", 7) == 0) {
        host += 7;
    } else if (strncmp(host, "https://", 8) == 0) {
        return ROM_API_ERR_PARAM;
    }

    host_len = strlen(host);
    while (host_len > 0 && (host[host_len - 1] == ' ' || host[host_len - 1] == '\t' ||
                            host[host_len - 1] == '/')) {
        host_len--;
    }
    if (host_len == 0 || host_len >= sizeof(tmp)) {
        return ROM_API_ERR_PARAM;
    }

    int n = (memchr(host, ':', host_len) != NULL)
                ? snprintf(tmp, sizeof(tmp), "http://%.*s", (int)host_len, host)
                : snprintf(tmp, sizeof(tmp), "http://%.*s:%d", (int)host_len, host, ROM_API_DEFAULT_PORT);
    if (n < 0 || n >= (int)sizeof(tmp) || (size_t)n >= len) {
        return ROM_API_ERR_PARAM;
    }

    memcpy(buf, tmp, (size_t)n + 1);
    return ROM_API_OK;
}

int rom_api_resolve_base(char *buf, size_t len)
{
    char *kv = NULL;

    if (!buf || len == 0) {
        return ROM_API_ERR_PARAM;
    }
    buf[0] = '\0';

    /* ① 显式配置优先: kv set string user.rom_api_url http://192.168.31.205:38202 */
    if (lisa_kv_get_string(KV_KEY_USER_ROM_API_URL, &kv) == 0 && kv) {
        int ret = (kv[0] != '\0') ? normalize_base(kv, buf, len) : ROM_API_ERR_PARAM;
        lisa_kv_free(kv);
        if (ret == ROM_API_OK) {
            return ROM_API_OK;
        }
        LISA_LOGW(TAG, "%s invalid, fallback to ws peer", KV_KEY_USER_ROM_API_URL);
    }

    /* ② 回退: 设备自己确认到的 PC 端地址 —— 先是已确认过的
     *    (pad_gui 的 UDP 发现探测来源 / WebSocket 手柄会话对端),
     *    都没有时主动广播探测服务端 (不要求 pad_gui 先扫描过设备)。
     *    端口优先用对端自报值。 */
    char ip[32];
    uint16_t port = 0;
    if (gamepad_find_server_addr(ip, sizeof(ip), &port)) {
        if (port == 0) {
            port = ROM_API_DEFAULT_PORT;
        }
        int n = snprintf(buf, len, "http://%s:%u", ip, (unsigned)port);
        if (n > 0 && (size_t)n < len) {
            return ROM_API_OK;
        }
    }

    buf[0] = '\0';
    return ROM_API_ERR_NO_BASE;
}

/* ------------------------------------------------------------------ */
/* URL 编码 / 响应解析                                                  */
/* ------------------------------------------------------------------ */

/* RFC3986 unreserved (A-Z a-z 0-9 - _ . ~) 原样, 其余字节按 %XX 转义。
 * 关键词多为中文, 必须编码后才能放进 query string。 */
static bool url_encode(const char *src, char *dst, size_t dst_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t di = 0;

    for (const uint8_t *p = (const uint8_t *)src; *p; p++) {
        const uint8_t c = *p;
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                                c == '.' || c == '~';
        if (unreserved) {
            if (di + 1 >= dst_len) {
                return false;
            }
            dst[di++] = (char)c;
        } else {
            if (di + 3 >= dst_len) {
                return false;
            }
            dst[di++] = '%';
            dst[di++] = hex[c >> 4];
            dst[di++] = hex[c & 0x0F];
        }
    }
    if (di >= dst_len) {
        return false;
    }
    dst[di] = '\0';
    return true;
}

static void copy_str(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) {
        return;
    }
    if (src) {
        snprintf(dst, dst_len, "%s", src);
    }
}

static void rom_on_data(lisa_http_data_t *data)
{
    cJSON **p_json = (cJSON **)data->user;

    /* 响应体在回调返回后立即释放, 必须在这里解析完 */
    if (!p_json || *p_json) {
        return;
    }
    *p_json = cJSON_ParseWithLength((const char *)data->buf, data->len);
    if (!*p_json) {
        LISA_LOGE(TAG, "rom api json parse failed (len=%d)", (int)data->len);
    }
}

static int rom_result_from_json(cJSON *root, rom_api_result_t *out)
{
    const cJSON *j_roms = cJSON_GetObjectItem(root, "roms");

    if (!cJSON_IsObject(root) || !cJSON_IsArray(j_roms)) {
        LISA_LOGE(TAG, "unexpected rom api response");
        return ROM_API_ERR_PARSE;
    }

    const cJSON *j_total = cJSON_GetObjectItem(root, "total");
    const cJSON *j_matched = cJSON_GetObjectItem(root, "matched");
    out->total = cJSON_IsNumber(j_total) ? (uint32_t)cJSON_GetNumberValue(j_total) : 0u;
    out->matched = cJSON_IsNumber(j_matched) ? (uint32_t)cJSON_GetNumberValue(j_matched) : 0u;

    int count = cJSON_GetArraySize(j_roms);
    if (count <= 0) {
        return ROM_API_OK;  /* 无匹配不是错误 */
    }

    out->items = lisa_mem_calloc((uint32_t)count, sizeof(rom_api_item_t));
    if (!out->items) {
        return ROM_API_ERR_NOMEM;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(j_roms, i);
        if (!cJSON_IsObject(item)) {
            continue;
        }

        rom_api_item_t *dst = &out->items[out->count];
        memset(dst, 0, sizeof(*dst));
        dst->mapper = ROM_API_MAPPER_INVALID;

        copy_str(dst->name, sizeof(dst->name), cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")));
        copy_str(dst->rel_path, sizeof(dst->rel_path),
                 cJSON_GetStringValue(cJSON_GetObjectItem(item, "rel_path")));
        copy_str(dst->ext, sizeof(dst->ext), cJSON_GetStringValue(cJSON_GetObjectItem(item, "ext")));

        const cJSON *j_size = cJSON_GetObjectItem(item, "size_kb");
        if (cJSON_IsNumber(j_size)) {
            dst->size_kb = (uint32_t)cJSON_GetNumberValue(j_size);
        }

        const cJSON *j_ines = cJSON_GetObjectItem(item, "ines");
        const cJSON *j_mapper = cJSON_IsObject(j_ines) ? cJSON_GetObjectItem(j_ines, "mapper") : NULL;
        if (cJSON_IsNumber(j_mapper)) {
            dst->mapper = (int)cJSON_GetNumberValue(j_mapper);
        }

        /* 无文件名/路径的条目对大模型没有意义, 丢弃 */
        if (dst->name[0] == '\0' && dst->rel_path[0] == '\0') {
            continue;
        }
        out->count++;
    }

    return ROM_API_OK;
}

/* ------------------------------------------------------------------ */
/* 查询                                                                 */
/* ------------------------------------------------------------------ */

int rom_api_search(const char *query, uint32_t limit, rom_api_result_t *out)
{
    char base[ROM_API_URL_MAX];
    char url[ROM_API_URL_MAX];
    char *enc = NULL;
    cJSON *json = NULL;
    int ret;

    if (!out) {
        return ROM_API_ERR_PARAM;
    }
    memset(out, 0, sizeof(*out));

    if (!query || query[0] == '\0' || limit == 0) {
        return ROM_API_ERR_PARAM;
    }

    ret = rom_api_resolve_base(base, sizeof(base));
    if (ret != ROM_API_OK) {
        return ret;
    }

    const size_t enc_len = strlen(query) * 3 + 1;
    enc = lisa_mem_calloc(1, (uint32_t)enc_len);
    if (!enc) {
        return ROM_API_ERR_NOMEM;
    }
    if (!url_encode(query, enc, enc_len)) {
        lisa_mem_free(enc);
        return ROM_API_ERR_PARAM;
    }

    /* with_ines=1: 需要 mapper 供大模型判断兼容性; pad_gui 侧按 (mtime,size) 缓存, 代价很小 */
    int n = snprintf(url, sizeof(url), "%s/api/roms?q=%s&limit=%u&with_ines=1", base, enc,
                     (unsigned)limit);
    lisa_mem_free(enc);
    if (n < 0 || n >= (int)sizeof(url)) {
        LISA_LOGW(TAG, "rom api url too long (%d)", n);
        return ROM_API_ERR_PARAM;
    }
    LISA_LOGD(TAG, "GET %s", url);

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .timeout = ROM_API_REQUEST_TIMEOUT_S,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,  /* 无自定义头; Host 由 httpclient 依 URL 自动补 */
        .on_data = rom_on_data,
        .user = &json,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        return ROM_API_ERR_HTTP;
    }

    lisa_http_err_e err = lisa_http_perform(http);
    lisa_http_cleanup(http);

    if (err != LISA_HTTP_OK) {
        LISA_LOGW(TAG, "rom api request failed: %d (%s)", err, base);
        if (json) {
            cJSON_Delete(json);
        }
        return ROM_API_ERR_NET;
    }
    if (!json) {
        return ROM_API_ERR_PARSE;
    }

    ret = rom_result_from_json(json, out);
    cJSON_Delete(json);
    if (ret != ROM_API_OK) {
        rom_api_result_free(out);
    }
    return ret;
}

void rom_api_result_free(rom_api_result_t *r)
{
    if (!r) {
        return;
    }
    if (r->items) {
        lisa_mem_free(r->items);
    }
    memset(r, 0, sizeof(*r));
}

/* ------------------------------------------------------------------ */
/* 详情 / 下载                                                          */
/* ------------------------------------------------------------------ */

/* 拼 "<base>/api/roms/<urlencoded rel_path>[suffix]"。 */
static int build_rom_url(const char *rel_path, const char *suffix, char *url, size_t url_len)
{
    char base[ROM_API_URL_MAX];
    char enc[ROM_API_PATH_ENC_MAX];
    int ret;

    if (!rel_path || rel_path[0] == '\0') {
        return ROM_API_ERR_PARAM;
    }

    ret = rom_api_resolve_base(base, sizeof(base));
    if (ret != ROM_API_OK) {
        return ret;
    }

    /* 中文路径逐字节转义后最长 3 倍; 底层 URI 缓冲仅 512B, 装不下就只能放弃 */
    if (!url_encode(rel_path, enc, sizeof(enc))) {
        LISA_LOGW(TAG, "rom path too long to encode (%u bytes)", (unsigned)strlen(rel_path));
        return ROM_API_ERR_PARAM;
    }

    int n = snprintf(url, url_len, "%s/api/roms/%s%s", base, enc, suffix ? suffix : "");
    if (n < 0 || (size_t)n >= url_len) {
        LISA_LOGW(TAG, "rom url too long (%d)", n);
        return ROM_API_ERR_PARAM;
    }
    return ROM_API_OK;
}

int rom_api_stat(const char *rel_path, rom_api_stat_t *out)
{
    char url[ROM_API_URL_MAX];
    cJSON *json = NULL;
    int ret;

    if (!out) {
        return ROM_API_ERR_PARAM;
    }
    memset(out, 0, sizeof(*out));
    out->mapper = ROM_API_MAPPER_INVALID;

    ret = build_rom_url(rel_path, "", url, sizeof(url));
    if (ret != ROM_API_OK) {
        return ret;
    }
    LISA_LOGD(TAG, "GET %s", url);

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .timeout = ROM_API_REQUEST_TIMEOUT_S,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,
        .on_data = rom_on_data,
        .user = &json,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        return ROM_API_ERR_HTTP;
    }
    lisa_http_err_e err = lisa_http_perform(http);
    lisa_http_cleanup(http);

    if (err != LISA_HTTP_OK) {
        LISA_LOGW(TAG, "rom stat request failed: %d", err);
        if (json) {
            cJSON_Delete(json);
        }
        return ROM_API_ERR_NET;
    }
    if (!json) {
        return ROM_API_ERR_PARSE;
    }

    /* 未知 id 时服务端回 {"error":..,"status":404}, 没有 size_bytes */
    const cJSON *j_size = cJSON_GetObjectItem(json, "size_bytes");
    if (!cJSON_IsNumber(j_size)) {
        LISA_LOGW(TAG, "rom not found or bad detail response: %s", rel_path);
        cJSON_Delete(json);
        return ROM_API_ERR_NOFILE;
    }

    out->size_bytes = (uint32_t)cJSON_GetNumberValue(j_size);
    copy_str(out->name, sizeof(out->name),
             cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")));
    copy_str(out->rel_path, sizeof(out->rel_path),
             cJSON_GetStringValue(cJSON_GetObjectItem(json, "rel_path")));
    copy_str(out->ext, sizeof(out->ext),
             cJSON_GetStringValue(cJSON_GetObjectItem(json, "ext")));

    const cJSON *j_ines = cJSON_GetObjectItem(json, "ines");
    const cJSON *j_mapper = cJSON_IsObject(j_ines) ? cJSON_GetObjectItem(j_ines, "mapper") : NULL;
    if (cJSON_IsNumber(j_mapper)) {
        out->mapper = (int)cJSON_GetNumberValue(j_mapper);
    }

    cJSON_Delete(json);
    return ROM_API_OK;
}

/* 下载回调的上下文: 统计总量 + 记住调用方是否已判定失败 */
typedef struct {
    rom_api_chunk_cb_t cb;
    void *user;
    uint32_t total;
    bool aborted;
} rom_dl_ctx_t;

static void rom_download_on_data(lisa_http_data_t *data)
{
    rom_dl_ctx_t *ctx = (rom_dl_ctx_t *)data->user;

    if (!ctx || data->len <= 0) {
        return;
    }
    ctx->total += (uint32_t)data->len;

    /* 已判定失败: 仍把响应读完 (lisa_http_download 不支持中途断开), 但不再回调 */
    if (ctx->aborted) {
        return;
    }
    if (ctx->cb((const uint8_t *)data->buf, (uint32_t)data->len, ctx->user) != 0) {
        LISA_LOGW(TAG, "rom download aborted by consumer at %u bytes", (unsigned)ctx->total);
        ctx->aborted = true;
    }
}

int rom_api_download(const char *rel_path, rom_api_chunk_cb_t cb, void *user, uint32_t *out_bytes)
{
    char url[ROM_API_URL_MAX];
    rom_dl_ctx_t ctx = { .cb = cb, .user = user, .total = 0, .aborted = false };
    int ret;

    if (out_bytes) {
        *out_bytes = 0;
    }
    if (!cb) {
        return ROM_API_ERR_PARAM;
    }

    ret = build_rom_url(rel_path, "/download", url, sizeof(url));
    if (ret != ROM_API_OK) {
        return ret;
    }
    LISA_LOGI(TAG, "GET %s", url);

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .timeout = ROM_API_DOWNLOAD_TIMEOUT_S,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,
        .on_data = rom_download_on_data,   /* 分块回调, 不是"整包一次" */
        .user = &ctx,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        return ROM_API_ERR_HTTP;
    }
    /* 用 download 而非 perform: 边收边喂, 不在内存里再存一份 (ROM 可达 1MB) */
    lisa_http_err_e err = lisa_http_download(http);
    lisa_http_cleanup(http);

    if (out_bytes) {
        *out_bytes = ctx.total;
    }
    if (ctx.aborted) {
        return ROM_API_ERR_ABORTED;
    }
    if (err != LISA_HTTP_OK) {
        LISA_LOGW(TAG, "rom download failed: %d (got %u bytes)", err, (unsigned)ctx.total);
        return ROM_API_ERR_NET;
    }
    return ROM_API_OK;
}
