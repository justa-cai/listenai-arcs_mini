#define TAG "ota_api"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_http.h"
#include "lisa_kv.h"
#include "lisa_log.h"
#include "cJSON.h"

#include "ota_api.h"
#include "kv_user.h"
#include "aiui_cfg.h"

#define DEV_CONF_API "api.listenai.com/external/device/configurations"

#define PRODUCT_ID_LEN 64
#define DEVICE_ID_LEN  32

static int get_product_id(char product_id[PRODUCT_ID_LEN])
{
    memset(product_id, 0, PRODUCT_ID_LEN);

    char *val = NULL;
    if (lisa_kv_get_string(KV_KEY_USER_PID, &val) == 0 && val != NULL) {
        strcpy(product_id, val);
        lisa_kv_free(val);
    } else {
        strcpy(product_id, PRODUCT_ID);
    }

    return 0;
}

static int get_device_id(char device_id[DEVICE_ID_LEN])
{
    memset(device_id, 0, DEVICE_ID_LEN);

    char did_buf[16];
    uint32_t *id_1 = (uint32_t *)0x48600208;
    uint32_t *id_2 = (uint32_t *)0x4860020c;

    char *val = NULL;
    if (lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &val) == 0 && val != NULL) {
        strcpy(device_id, val);
        lisa_kv_free(val);
    } else {
        if (*id_1 == 0 && *id_2 == 0) {
            return -1;
        }

        memcpy(did_buf + 0, id_1, sizeof(uint32_t));
        memcpy(did_buf + 4, id_2, sizeof(uint32_t));
        for (int i = 0; i < 8; i++) {
            sprintf(&device_id[i * 2], "%02x", (uint8_t)did_buf[i]);
        }
    }

    return 0;
}

static void *conf_headers(void)
{
    return "Accept: application/json";
}

static void conf_on_data(lisa_http_data_t *data)
{
    LISA_LOGI(TAG, "Config data received: %.*s", data->len, (const char *)data->buf);

    cJSON **p_json = (cJSON **)data->user;
    *p_json = cJSON_ParseWithLength((const char *)data->buf, data->len);
    if (!*p_json) {
        LISA_LOGE(TAG, "Failed to parse JSON");
    }
}

static cJSON *conf_get(void)
{
    char product_id[PRODUCT_ID_LEN];
    char device_id[DEVICE_ID_LEN];
    char url[256];
    int ret;

    ret = get_product_id(product_id);
    if (ret < 0) {
        LISA_LOGE(TAG, "Get product ID failed: %d", ret);
        return NULL;
    }

    ret = get_device_id(device_id);
    if (ret < 0) {
        LISA_LOGE(TAG, "Get device ID failed: %d", ret);
        return NULL;
    }

    int device_mode = 0;
    ret = lisa_kv_get_int(KV_KEY_STAGING, &device_mode);
    if (ret != 0) {
        device_mode = 0;
    }

    const char *host_suffix = "";
    if (device_mode == 1) {
        host_suffix = "staging-";
    } else if (device_mode == 2) {
        host_suffix = "integration-";
    }

    snprintf(url, sizeof(url), "http://%s%s?product_id=%s&device_id=%s", host_suffix, DEV_CONF_API, product_id,
             device_id);

    cJSON *json = NULL;

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = url,
        .timeout = 10,
        .body = NULL,
        .body_len = 0,
        .headers = (uint8_t *)conf_headers,
        .on_data = conf_on_data,
        .user = &json,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "HTTP init failed");
        return NULL;
    }

    lisa_http_err_e err = lisa_http_perform(http);
    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "HTTP perform failed: %d", err);
        lisa_http_cleanup(http);
        return NULL;
    }

    lisa_http_cleanup(http);

    return json;
}

static int ota_api_parse_res_info(cJSON *item, ota_res_info_t *target)
{
    if (!item || !target) {
        return -1;
    }

    memset(target, 0, sizeof(ota_res_info_t));

    cJSON *size = cJSON_GetObjectItem(item, "size");
    cJSON *md5 = cJSON_GetObjectItem(item, "md5");
    cJSON *url = cJSON_GetObjectItem(item, "url");
    if (!cJSON_IsNumber(size) || !cJSON_IsString(md5) || !cJSON_IsString(url)) {
        return -1;
    }

    char *url_val = cJSON_GetStringValue(url);
    if (strlen(url_val) >= OTA_RES_URL_LEN) {
        LISA_LOGE(TAG, "URL too long: %s", url_val);
        return -1;
    }

    for (char *s = cJSON_GetStringValue(md5), *d = target->md5; *s && d < &target->md5[OTA_RES_MD5_LEN]; d++, s += 2) {
        sscanf(s, "%2hhx", d);
    }

    strncpy(target->url, url_val, OTA_RES_URL_LEN - 1);
    target->url[OTA_RES_URL_LEN - 1] = '\0';

    target->size = (uint32_t)cJSON_GetNumberValue(size);

    return 0;
}

int ota_api_get_dev_conf(ota_dev_conf_t *conf)
{
    if (!conf) {
        return -1;
    }

    memset(conf, 0, sizeof(ota_dev_conf_t));

    cJSON *json = conf_get();
    if (!json) {
        LISA_LOGE(TAG, "Get config failed");
        return -1;
    }

    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (!data) {
        LISA_LOGE(TAG, "No data field in JSON");
        cJSON_Delete(json);
        return -1;
    }

    cJSON *wakeup_word = cJSON_GetObjectItem(data, "wakeup_word");
    if (wakeup_word) {
        cJSON *resources = cJSON_GetObjectItem(wakeup_word, "resources");
        if (resources) {
            for (int i = 0; i < cJSON_GetArraySize(resources); i++) {
                cJSON *item = cJSON_GetArrayItem(resources, i);
                if (!item) {
                    continue;
                }

                char *name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
                if (!name) {
                    continue;
                }

                ota_res_info_t *target = NULL;
                if (strcmp(name, "wake_word.bin") == 0) {
                    target = &conf->wakeup_word.resource;
                } else {
                    continue;
                }

                if (ota_api_parse_res_info(item, target) != 0) {
                    LISA_LOGE(TAG, "Failed to parse resource info for %s", name);
                    continue;
                }

                LISA_LOGI(TAG, "Found wake word resource (%s), size: %d, md5: " MD5_PRI ", url: %s", name, target->size,
                          MD5_ARG(target->md5), target->url);
            }
        }

        char *text = cJSON_GetStringValue(cJSON_GetObjectItem(wakeup_word, "text"));
        if (text) {
            memset(conf->wakeup_word.text, 0, sizeof(conf->wakeup_word.text));
            strncpy(conf->wakeup_word.text, text, sizeof(conf->wakeup_word.text) - 1);
            LISA_LOGI(TAG, "Wakeup word text: %s", conf->wakeup_word.text);
        }
    }

    cJSON *greeting = cJSON_GetObjectItem(data, "greeting");
    if (ota_api_parse_res_info(greeting, &conf->greeting) != 0) {
        LISA_LOGE(TAG, "Failed to parse greeting resource info");
    } else {
        LISA_LOGI(TAG, "Found greeting resource, size: %d, md5: " MD5_PRI ", url: %s", conf->greeting.size,
                  MD5_ARG(conf->greeting.md5), conf->greeting.url);
    }

    cJSON_Delete(json);

    return 0;
}

struct download_context {
    const ota_res_info_t *res_info;
    ota_download_cb_t cb;
    int cb_ret;
    uint32_t downloaded;
};

static void download_on_data(lisa_http_data_t *data)
{
    struct download_context *ctx = (struct download_context *)data->user;
    const ota_res_info_t *res_info = ctx->res_info;

    if (ctx->cb_ret < 0 || !data->len) {
        return;
    }

    ctx->downloaded += data->len;

    if (ctx->cb) {
        ctx->cb_ret = ctx->cb(res_info, ctx->downloaded - data->len, (const uint8_t *)data->buf, (uint32_t)data->len);
    }
}

int ota_api_download(const ota_res_info_t *res_info, ota_download_cb_t cb)
{
    struct download_context ctx = {
        .res_info = res_info,
        .cb = cb,
        .cb_ret = 0,
        .downloaded = 0,
    };

    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)res_info->url,
        .timeout = 10,
        .body = NULL,
        .body_len = 0,
        .headers = NULL,
        .on_data = download_on_data,
        .user = (void *)&ctx,
    };

    lisa_http_t *http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "HTTP init failed");
        return -1;
    }

    lisa_http_err_e err = lisa_http_download(http);
    if (err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "HTTP download failed: %d", err);
        lisa_http_cleanup(http);
        return -1;
    }

    if (ctx.cb_ret < 0) {
        LISA_LOGE(TAG, "Download completed with error from callback: %d", ctx.cb_ret);
        lisa_http_cleanup(http);
        return ctx.cb_ret;
    }

    LISA_LOGI(TAG, "Download completed: %u bytes", ctx.downloaded);

    lisa_http_cleanup(http);

    return ctx.downloaded;
}
