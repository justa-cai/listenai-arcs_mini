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
#include "lisa_mem.h"
#include "cJSON.h"
#include "aiui_base64.h"
#include "project_version.h"

#include "ota_api.h"
#include "kv_user.h"
#include "app_datas.h"

#define DEV_CONF_API "api.listenai.com/external/device/configurations"

#define CONF_URL_LEN 512

static char *conf_build_param_base64(void)
{
    cJSON *params = cJSON_CreateObject();
    if (!params) {
        return NULL;
    }

    cJSON *firmware_info = cJSON_CreateObject();
    if (!firmware_info) {
        cJSON_Delete(params);
        return NULL;
    }

    cJSON_AddStringToObject(firmware_info, "type", "arcs-mini");
    cJSON_AddStringToObject(firmware_info, "version", PROJECT_VERSION_STR);
    cJSON_AddItemToObject(params, "firmware_info", firmware_info);

    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        return NULL;
    }

    char *params_base64 = (char *)aiui_base64_encode((unsigned char *)params_json);
    cJSON_free(params_json);
    if (!params_base64 || params_base64[0] == '\0') {
        if (params_base64) {
            lisa_mem_free(params_base64);
        }
        return NULL;
    }

    return params_base64;
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
    struct app_datas *app_datas = get_app_datas();
    char url[CONF_URL_LEN];
    int ret;

    const char *host_suffix = "";
    if (app_datas->device_mode == DEVICE_MODE_STAGING) {
        host_suffix = "staging-";
    } else if (app_datas->device_mode == DEVICE_MODE_INTEGRATION) {
        host_suffix = "integration-";
    }

    char *param_base64 = conf_build_param_base64();
    int url_len = 0;
    if (param_base64) {
        url_len = snprintf(url, sizeof(url), "http://%s%s?product_id=%s&device_id=%s&param=%s", host_suffix,
                           DEV_CONF_API, app_datas->pid, app_datas->did, param_base64);
    } else {
        LISA_LOGW(TAG, "param base64 build failed, request without param");
        url_len = snprintf(url, sizeof(url), "http://%s%s?product_id=%s&device_id=%s", host_suffix, DEV_CONF_API,
                           app_datas->pid, app_datas->did);
    }

    if (param_base64) {
        lisa_mem_free(param_base64);
        param_base64 = NULL;
    }

    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        LISA_LOGE(TAG, "Config URL too long");
        return NULL;
    }

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

    cJSON *prompt_tone = cJSON_GetObjectItem(data, "prompt_tone");
    if (ota_api_parse_res_info(prompt_tone, &conf->prompt_tone) != 0) {
        LISA_LOGE(TAG, "Failed to parse prompt_tone resource info");
    } else {
        LISA_LOGI(TAG, "Found prompt_tone resource, size: %d, md5: " MD5_PRI ", url: %s", conf->prompt_tone.size,
                  MD5_ARG(conf->prompt_tone.md5), conf->prompt_tone.url);
    }

    cJSON *emoji = cJSON_GetObjectItem(data, "emoji");
    if (ota_api_parse_res_info(emoji, &conf->emoji) != 0) {
        LISA_LOGE(TAG, "Failed to parse emoji resource info");
    } else {
        LISA_LOGI(TAG, "Found emoji resource, size: %d, md5: " MD5_PRI ", url: %s", conf->emoji.size,
                  MD5_ARG(conf->emoji.md5), conf->emoji.url);
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
