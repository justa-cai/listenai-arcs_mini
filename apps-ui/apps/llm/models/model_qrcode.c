/**
 * @file model_qrcode.c
 * @brief QR code data model implementation
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "model_qrcode"

#include "lisa_ui.h"
#include "lisa_ui_invoke.h"

#include "model_wifi.h"
#include "model_qrcode.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "bt_app_if.h"
#include "lisa_thread.h"
#include "HTTPCUsr_api.h"
#include "voice_msg.h"
#include "lvgl.h"
#include "cJSON.h"
#include "app_datas.h"
#include "lisa_ui_invoke.h"
#include "app_net_cfg.h"
#endif

LV_IMG_DECLARE(ble_qr);

/**
 * @brief QR code model context
 */
typedef struct {
    bool initialized;
    qrcode_status_t status;
    const char *top_text;
    const char *bottom_text;
    const void *qr_image;
    lv_img_dsc_t dynamic_qrcode_img;
    char role_setting_qrcode_url[512];
    uint8_t *role_setting_qrcode_img_raw;
    uint32_t role_setting_qrcode_img_raw_size;
    char device_id[64];
    char device_id_text[72];
    
    char quota_qrcode_url[512];
    uint8_t *quota_qrcode_img_raw;
    uint32_t quota_qrcode_img_raw_size;
    char quota_message[256];
    char quota_err_code[64];
    lv_img_dsc_t quota_qrcode_img;
} qrcode_model_ctx_t;

static qrcode_model_ctx_t g_qrcode_ctx = {0};

// Default text strings
static const char *DEFAULT_NOT_CONNECTED_TEXT = "当前网络未连接\n请使用微信扫码配网";
static const char *DEFAULT_CONNECTED_TEXT = "请使用微信扫码修改配置";
static const char *DEFAULT_OUT_OF_LIMIT_TEXT = "今日交互额度已用完";
static const char *DEFAULT_OUT_OF_LIMIT_BOTTOM_TEXT = "微信扫码开通会员\n获取更多交互额度";

#ifdef LISA_UI_PLATFORM_ARCS
static void qrcode_download_thread(void *arg)
{
    char *url = (char *)arg;
    int ret = 0;
    uint8_t *download_buffer = NULL;
    uint32_t buffer_size = 0;
    unsigned int received = 0;
    unsigned int total_received = 0;

    HTTPParameters *http_param = (HTTPParameters *)lisa_ui_malloc(sizeof(HTTPParameters));
    if (!http_param) {
        LISA_UI_LOGE("Failed to allocate HTTP parameters");
        return;
    }
    memset(http_param, 0, sizeof(HTTPParameters));

    if (strncmp(url, "https", 5) == 0) {
        strcpy(http_param->Uri, "http");
        strcat(http_param->Uri, url + 5);
        LISA_UI_LOGI("Convert HTTPS to HTTP: %s -> %s", url, http_param->Uri);
    } else {
        strcpy(http_param->Uri, url);
    }

    http_param->HttpVerb = VerbGet;
    http_param->nTimeout = 30;
    http_param->pData = NULL;
    http_param->pLength = 0;

    if ((ret = HTTPC_open(http_param)) != 0) {
        LISA_UI_LOGE("Failed to open HTTP connection: %d", ret);
        goto cleanup;
    }

    if ((ret = HTTPC_request(http_param, NULL)) != 0) {
        LISA_UI_LOGE("Failed to send HTTP request: %d", ret);
        goto cleanup;
    }

    HTTP_CLIENT http_client = {0};
    if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
        LISA_UI_LOGE("Failed to get HTTP response info: %d", ret);
        goto cleanup;
    }

    if (http_client.TotalResponseBodyLength == 0) {
        LISA_UI_LOGE("Empty response body");
        goto cleanup;
    }

    buffer_size = http_client.TotalResponseBodyLength;
    download_buffer = (uint8_t *)lisa_ui_malloc(buffer_size);
    if (!download_buffer) {
        LISA_UI_LOGE("Failed to allocate download buffer: %u bytes", buffer_size);
        goto cleanup;
    }

    do {
        unsigned int remaining = buffer_size - total_received;
        unsigned int to_read = remaining;
        UINT32 received = 0;
        ret = HTTPC_read(http_param, download_buffer + total_received, to_read, &received);

        if (received > 0) {
            total_received += received;
        }

        if (ret != 0) {
            break;
        }
        vTaskDelay(2);
    } while (total_received < buffer_size);

    if (total_received == buffer_size) {
        LISA_UI_LOGI("QR code download completed successfully: %u bytes, %p", total_received, download_buffer);
        struct invoke_payload {
            uint8_t *img;
            uint32_t len;
        };

        struct invoke_payload payload = {
            .img = download_buffer,
            .len = total_received,
        };

        struct invoke_payload *p = &payload;
        uint32_t p_len = sizeof(struct invoke_payload);

        LISA_UI_INVOKE_UI_ARG_PTR(p, p_len, {
            if (g_qrcode_ctx.role_setting_qrcode_img_raw) {
                lisa_ui_free(g_qrcode_ctx.role_setting_qrcode_img_raw);
            }
            LISA_UI_LOGI("set role setting qrcode, img:%p, size: %d", _invoke_p->img, _invoke_p->len);
            g_qrcode_ctx.role_setting_qrcode_img_raw = _invoke_p->img;
            g_qrcode_ctx.role_setting_qrcode_img_raw_size = _invoke_p->len;
        });
    } else {
        LISA_UI_LOGE("Download incomplete: %u/%u bytes", total_received, buffer_size);
    }

cleanup:
    HTTPC_close(http_param);

    if (http_param) {
        lisa_ui_free(http_param);
    }

    if (download_buffer) {
        /* 不需要释放download_buffer */
    }

    LISA_UI_LOGD("QR code download thread exiting");
}

static int modify_qrcode_url(const char *input_url, char *output_url, size_t output_size)
{
    if (!input_url || !output_url || output_size == 0) {
        LISA_UI_LOGE("Invalid parameters");
        return -1;
    }

    const char *question_mark = strchr(input_url, '?');
    if (question_mark) {
        size_t base_url_len = question_mark - input_url + 1;
        const char *new_params = "x-oss-process=image/resize,m_pad,w_148,limit_0,color_000000/quality,q_100/format,jpg";
        size_t new_params_len = strlen(new_params);

        if (base_url_len + new_params_len + 1 > output_size) {
            LISA_UI_LOGE("Output buffer too small");
            return -1;
        }

        memcpy(output_url, input_url, base_url_len);
        strcpy(output_url + base_url_len, new_params);
        LISA_UI_LOGI("Modified URL: %s -> %s", input_url, output_url);

        return 0;
    } else {
        const char *new_params = "x-oss-process=image/resize,m_pad,w_148,limit_0,color_000000/quality,q_100/format,jpg";
        size_t input_len = strlen(input_url);
        size_t new_params_len = strlen(new_params);
        size_t total_len = input_len + 1 + new_params_len + 1;

        if (total_len > output_size) {
            LISA_UI_LOGE("Output buffer too small for modified URL");
            return -1;
        }

        strcpy(output_url, input_url);
        output_url[input_len] = '?';
        strcpy(output_url + input_len + 1, new_params);
        LISA_UI_LOGI("Modified URL (add params): %s -> %s", input_url, output_url);
        return 0;
    }
}

static void download_qrcode_start(void)
{
    static uint8_t qr_download_name[] = "qr_download";
    lisa_thread_attr_t thread_attr = {
        .name = qr_download_name,
        .stack_size = 1024 * 16,
        .priority = 5,
    };

    lisa_thread_t *download_thread =
        lisa_thread_create(&thread_attr, qrcode_download_thread, (void *)g_qrcode_ctx.role_setting_qrcode_url);

    if (!download_thread) {
        LISA_UI_LOGE("Failed to create QR code download thread");
    } else {
        LISA_UI_LOGD("QR code download thread created successfully");
    }
}

static void voice_cloud_role_setting_qrcode_handle(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                                   void *user_data)
{
    if (!data || len == 0 || len >= sizeof(g_qrcode_ctx.role_setting_qrcode_url)) {
        LISA_UI_LOGE("Invalid QR code URL data");
        return;
    }

    if (g_qrcode_ctx.status == QR_STATUS_OUT_OF_LIMIT) {
        LISA_UI_LOGI("Ignore role setting qrcode due to out-of-limit status");
        return;
    }

    char *url = (char *)data;

    LISA_UI_INVOKE_UI_ARG_PTR(url, len, {
        int ret = modify_qrcode_url(_invoke_url, g_qrcode_ctx.role_setting_qrcode_url,
                                    sizeof(g_qrcode_ctx.role_setting_qrcode_url));

        if (ret == 0) {
            download_qrcode_start();
        } else {
            LISA_UI_LOGE("Failed to modify QR code URL");
        }
    });
}

static void quota_qrcode_download_thread(void *arg)
{
    char *url = (char *)arg;
    int ret = 0;
    uint8_t *download_buffer = NULL;
    uint32_t buffer_size = 0;
    unsigned int total_received = 0;

    HTTPParameters *http_param = (HTTPParameters *)lisa_ui_malloc(sizeof(HTTPParameters));
    if (!http_param) {
        LISA_UI_LOGE("Failed to allocate HTTP parameters");
        return;
    }
    memset(http_param, 0, sizeof(HTTPParameters));

    if (strncmp(url, "https", 5) == 0) {
        strcpy(http_param->Uri, "http");
        strcat(http_param->Uri, url + 5);
        LISA_UI_LOGI("Convert HTTPS to HTTP: %s -> %s", url, http_param->Uri);
    } else {
        strcpy(http_param->Uri, url);
    }

    http_param->HttpVerb = VerbGet;
    http_param->nTimeout = 30;
    http_param->pData = NULL;
    http_param->pLength = 0;

    if ((ret = HTTPC_open(http_param)) != 0) {
        LISA_UI_LOGE("Failed to open HTTP connection: %d", ret);
        goto cleanup;
    }

    if ((ret = HTTPC_request(http_param, NULL)) != 0) {
        LISA_UI_LOGE("Failed to send HTTP request: %d", ret);
        goto cleanup;
    }

    HTTP_CLIENT http_client = {0};
    if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
        LISA_UI_LOGE("Failed to get HTTP response info: %d", ret);
        goto cleanup;
    }

    if (http_client.TotalResponseBodyLength == 0) {
        LISA_UI_LOGE("Empty response body");
        goto cleanup;
    }

    buffer_size = http_client.TotalResponseBodyLength;
    download_buffer = (uint8_t *)lisa_ui_malloc(buffer_size);
    if (!download_buffer) {
        LISA_UI_LOGE("Failed to allocate download buffer: %u bytes", buffer_size);
        goto cleanup;
    }

    do {
        unsigned int remaining = buffer_size - total_received;
        unsigned int to_read = remaining;
        UINT32 received = 0;
        ret = HTTPC_read(http_param, download_buffer + total_received, to_read, &received);

        if (received > 0) {
            total_received += received;
        }

        if (ret != 0) {
            break;
        }
        vTaskDelay(2);
    } while (total_received < buffer_size);

    if (total_received == buffer_size) {
        LISA_UI_LOGI("Quota QR code download completed: %u bytes, %p", total_received, download_buffer);
        
        struct invoke_payload {
            uint8_t *img;
            uint32_t len;
        };

        struct invoke_payload payload = {
            .img = download_buffer,
            .len = total_received,
        };

        struct invoke_payload *p = &payload;
        uint32_t p_len = sizeof(struct invoke_payload);

        LISA_UI_INVOKE_UI_ARG_PTR(p, p_len, {
            uint8_t *old_img = g_qrcode_ctx.quota_qrcode_img_raw;
            
            LISA_UI_LOGI("Set quota qrcode, img:0x%p, size: %u", _invoke_p->img, _invoke_p->len);
            g_qrcode_ctx.quota_qrcode_img_raw = _invoke_p->img;
            g_qrcode_ctx.quota_qrcode_img_raw_size = _invoke_p->len;
            
            memset(&g_qrcode_ctx.quota_qrcode_img, 0, sizeof(lv_img_dsc_t));
            g_qrcode_ctx.quota_qrcode_img.header.cf = LV_IMG_CF_RAW;
            g_qrcode_ctx.quota_qrcode_img.header.always_zero = 0;
            g_qrcode_ctx.quota_qrcode_img.header.reserved = 0;
            g_qrcode_ctx.quota_qrcode_img.header.w = 0;
            g_qrcode_ctx.quota_qrcode_img.header.h = 0;
            g_qrcode_ctx.quota_qrcode_img.data_size = g_qrcode_ctx.quota_qrcode_img_raw_size;
            g_qrcode_ctx.quota_qrcode_img.data = g_qrcode_ctx.quota_qrcode_img_raw;
            
            if (old_img) {
                LISA_UI_LOGI("Freeing old quota qrcode image: 0x%p", old_img);
                lisa_ui_free(old_img);
            }
        });
        
        download_buffer = NULL;
    } else {
        LISA_UI_LOGE("Quota QR code download incomplete: %u/%u bytes", total_received, buffer_size);
    }

cleanup:
    if (download_buffer) {
        lisa_ui_free(download_buffer);
    }
    if (http_param) {
        HTTPC_close(http_param);
        lisa_ui_free(http_param);
    }
}

static void download_quota_qrcode_start(void)
{
    static uint8_t quota_qr_dl_name[] = "quota_qr_dl";
    lisa_thread_attr_t attr = {
        .name = quota_qr_dl_name,
        .priority = 8,
        .stack_size = 8192,
    };

    lisa_thread_create(&attr, quota_qrcode_download_thread, g_qrcode_ctx.quota_qrcode_url);
}

struct auth_failed_rsp {
    int err;
    uint8_t auth_failed;
};

static void get_auth_failed_worker(void *data, uint32_t len, struct voice_invoke_rsp *rsp)
{
    struct auth_failed_rsp *auth_rsp = (struct auth_failed_rsp *)rsp;
    struct app_datas *app_datas = get_app_datas();

    if (app_datas) {
        auth_rsp->err = 0;
        auth_rsp->auth_failed = app_datas->auth_failed;
    } else {
        auth_rsp->err = -1;
        auth_rsp->auth_failed = 0;
    }
}

static void voice_cloud_show_qrcode_handle(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                          void *user_data)
{
    if (!data || len == 0) {
        LISA_UI_LOGE("Invalid show qrcode data");
        return;
    }

    char *json_str = (char *)data;
    LISA_UI_LOGI("Show qrcode received: %s", json_str);

    LISA_UI_INVOKE_UI_ARG_PTR(json_str, len, {
        cJSON *json = cJSON_Parse(_invoke_json_str);
        if (!json) {
            LISA_UI_LOGE("Failed to parse show qrcode JSON");
            return;
        }

        cJSON *url_json = cJSON_GetObjectItem(json, "url");
        cJSON *message_json = cJSON_GetObjectItem(json, "message");
        cJSON *err_code_json = cJSON_GetObjectItem(json, "err_code");

        char url_buf[512] = {0};
        char message_buf[256] = {0};
        char err_code_buf[64] = {0};

        if (url_json && url_json->valuestring) {
            strncpy(url_buf, url_json->valuestring, sizeof(url_buf) - 1);
        }

        if (message_json && message_json->valuestring) {
            strncpy(message_buf, message_json->valuestring, sizeof(message_buf) - 1);
        } else {
            strncpy(message_buf, DEFAULT_OUT_OF_LIMIT_TEXT, sizeof(message_buf) - 1);
        }

        if (err_code_json && err_code_json->valuestring) {
            strncpy(err_code_buf, err_code_json->valuestring, sizeof(err_code_buf) - 1);
        }

        cJSON_Delete(json);

        if (url_buf[0] == '\0') {
            LISA_UI_LOGE("No URL in show qrcode JSON");
            return;
        }

        LISA_UI_LOGI("Parsed - url: %s, message: %s, err_code: %s", url_buf, message_buf, err_code_buf);

        int ret = modify_qrcode_url(url_buf, g_qrcode_ctx.quota_qrcode_url,
                                   sizeof(g_qrcode_ctx.quota_qrcode_url));

        if (ret == 0) {
            strncpy(g_qrcode_ctx.quota_message, message_buf,
                   sizeof(g_qrcode_ctx.quota_message) - 1);
            g_qrcode_ctx.quota_message[sizeof(g_qrcode_ctx.quota_message) - 1] = '\0';

            strncpy(g_qrcode_ctx.quota_err_code, err_code_buf,
                   sizeof(g_qrcode_ctx.quota_err_code) - 1);
            g_qrcode_ctx.quota_err_code[sizeof(g_qrcode_ctx.quota_err_code) - 1] = '\0';

            g_qrcode_ctx.status = QR_STATUS_OUT_OF_LIMIT;
            download_quota_qrcode_start();
            LISA_UI_LOGI("Quota qrcode download started");
        } else {
            LISA_UI_LOGE("Failed to modify quota QR code URL");
        }
    });
}
#endif

int model_qrcode_init(void)
{
    if (g_qrcode_ctx.initialized) {
        return 0;
    }

    g_qrcode_ctx.initialized = true;
#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_sub(VOICE_MSG_CLOUD_ROLE_SETTING_QRCODE, voice_cloud_role_setting_qrcode_handle, NULL);
    voice_msg_sub(VOICE_MSG_CLOUD_SHOW_QRCODE, voice_cloud_show_qrcode_handle, NULL);
    LISA_UI_LOGI("Subscribed to VOICE_MSG_CLOUD_SHOW_QRCODE");
#endif

    return 0;
}

qrcode_status_t model_qrcode_get_status(void)
{
    return g_qrcode_ctx.status;
}

int model_qrcode_get_quota_data(qrcode_data_t *data)
{
    if (!data) {
        LISA_UI_LOGE("Invalid parameter");
        return -1;
    }

    if (!g_qrcode_ctx.initialized) {
        LISA_UI_LOGW("QR code model not initialized, initializing now");
        model_qrcode_init();
    }

    data->status = QR_STATUS_OUT_OF_LIMIT;
    data->top_text = g_qrcode_ctx.quota_message[0] ? g_qrcode_ctx.quota_message : DEFAULT_OUT_OF_LIMIT_TEXT;
    data->bottom_text = DEFAULT_OUT_OF_LIMIT_BOTTOM_TEXT;
    data->qr_image = g_qrcode_ctx.quota_qrcode_img_raw ? &g_qrcode_ctx.quota_qrcode_img : NULL;

    LISA_UI_LOGI("Returning quota qrcode data, top_text: %s, bottom_text: %s, img: %p",
                 data->top_text, data->bottom_text, data->qr_image);
    return 0;
}

int model_qrcode_get_config_data(qrcode_data_t *data)
{
    if (!data) {
        LISA_UI_LOGE("Invalid parameter");
        return -1;
    }

    if (!g_qrcode_ctx.initialized) {
        LISA_UI_LOGW("QR code model not initialized, initializing now");
        model_qrcode_init();
    }

    model_wifi_status_t wifi_sta = model_wifi_get_status();

    if (wifi_sta == MODEL_WIFI_STATUS_DISCONNECTED || wifi_sta == MODEL_WIFI_STATUS_UNKNOWN) {
        data->status = QR_STATUS_NOT_CONNECTED;
        data->top_text = DEFAULT_NOT_CONNECTED_TEXT;
        if (get_current_device_id(g_qrcode_ctx.device_id, sizeof(g_qrcode_ctx.device_id)) == 0) {
            snprintf(g_qrcode_ctx.device_id_text, sizeof(g_qrcode_ctx.device_id_text),
                     "ID: %s", g_qrcode_ctx.device_id);
            data->bottom_text = g_qrcode_ctx.device_id_text;
        } else {
            data->bottom_text = "";
        }
        data->qr_image = &ble_qr;
#ifdef LISA_UI_PLATFORM_ARCS
        app_ble_adv_start(0, BLE_ADV_GEN);
#endif
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    struct auth_failed_rsp auth_rsp = {
        .err = -1,
        .auth_failed = 0,
    };

    int r = voice_invoke_sync(get_auth_failed_worker, NULL, 0,
                              (struct voice_invoke_rsp *)&auth_rsp, 1000);

    if (r == 0 && auth_rsp.err == 0 && auth_rsp.auth_failed) {
        data->top_text = "云端鉴权失败";
        data->bottom_text = "请联系技术对接人添加云端授权";
        data->qr_image = NULL;
        return 0;
    }
#endif

    data->status = QR_STATUS_CONNECTED;
    if (g_qrcode_ctx.role_setting_qrcode_img_raw == NULL) {
        data->top_text = "二维码加载中, 请稍后";
        data->bottom_text = "";
        data->qr_image = NULL;
        return 0;
    }

    g_qrcode_ctx.dynamic_qrcode_img.header.cf = LV_IMG_CF_RAW;
    g_qrcode_ctx.dynamic_qrcode_img.header.always_zero = 0;
    g_qrcode_ctx.dynamic_qrcode_img.header.reserved = 0;
    g_qrcode_ctx.dynamic_qrcode_img.header.w = 0;
    g_qrcode_ctx.dynamic_qrcode_img.header.h = 0;
    g_qrcode_ctx.dynamic_qrcode_img.data_size = g_qrcode_ctx.role_setting_qrcode_img_raw_size;
    g_qrcode_ctx.dynamic_qrcode_img.data = g_qrcode_ctx.role_setting_qrcode_img_raw;

    data->top_text = DEFAULT_CONNECTED_TEXT;
    data->bottom_text = "";
    data->qr_image = &g_qrcode_ctx.dynamic_qrcode_img;
    return 0;
}

int model_qrcode_get_data(qrcode_data_t *data)
{
    if (!data) {
        LISA_UI_LOGE("Invalid parameter");
        return -1;
    }

    if (!g_qrcode_ctx.initialized) {
        LISA_UI_LOGW("QR code model not initialized, initializing now");
        model_qrcode_init();
    }

    if (g_qrcode_ctx.status == QR_STATUS_OUT_OF_LIMIT) {
        return model_qrcode_get_quota_data(data);
    }

    return model_qrcode_get_config_data(data);
}
