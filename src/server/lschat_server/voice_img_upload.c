#include "lisa_log.h"
#include "HTTPCUsr_api.h"
#include "cJSON.h"
#include "lisa_mem.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "app_datas.h"
#include <string.h>
#include <stdio.h>

#define TAG "img_upload"

#define API_UPLOAD_PATH "/v1/device/assets"
#define VOICE_IMG_UPLOAD_TIMEOUT_SEC 10
#define VOICE_IMG_UPLOAD_MAX_ATTEMPTS 3
#define VOICE_IMG_UPLOAD_RETRY_DELAY_MS 1000
#define VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE HTTP_CLIENT_BUFFER_SIZE

static char *auth_header = NULL;
static volatile char *upload_headers = NULL;
static StaticSemaphore_t upload_lock_storage;
static SemaphoreHandle_t upload_lock = NULL;

struct voice_img_upload_post_ctx {
    const uint8_t *body;
    uint32_t body_len;
    uint32_t offset;
    /* The SDK measures the initial callback-backed block with strlen(). */
    char head_chunk[VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE + 1];
};

static struct voice_img_upload_post_ctx *s_voice_img_upload_post_ctx;

extern uint8_t *voice_token_get(void);

static SemaphoreHandle_t voice_img_upload_lock_get(void)
{
    if (upload_lock != NULL) {
        return upload_lock;
    }

    taskENTER_CRITICAL();
    if (upload_lock == NULL) {
        upload_lock = xSemaphoreCreateMutexStatic(&upload_lock_storage);
    }
    taskEXIT_CRITICAL();

    return upload_lock;
}

struct response_data {
    char *url;
    int status_code;
};

static void* http_get_headers_callback(void)
{
    if (upload_headers == NULL) {
        LOGE("upload_headers is NULL in callback!");
        return NULL;
    }
    LOGI("Headers callback returning: %p", upload_headers);
    return (void*)upload_headers;
}

static int voice_img_upload_prime_post_ctx(struct voice_img_upload_post_ctx *post_ctx,
                                           const uint8_t *body,
                                           uint32_t body_len,
                                           uint32_t header_len)
{
    if (!post_ctx || !body || body_len == 0 || header_len == 0 ||
        header_len > body_len || header_len > VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE) {
        return -1;
    }

    memset(post_ctx, 0, sizeof(*post_ctx));
    post_ctx->body = body;
    post_ctx->body_len = body_len;
    post_ctx->offset = header_len;
    /* Keep binary JPEG bytes out of the initial strlen()-measured block. */
    memcpy(post_ctx->head_chunk, body, header_len);
    post_ctx->head_chunk[header_len] = '\0';
    return 0;
}

static PostData voice_img_upload_get_post_data(void)
{
    PostData post_data = {.pData = NULL, .pLength = 0};
    struct voice_img_upload_post_ctx *post_ctx = s_voice_img_upload_post_ctx;
    uint32_t remaining;

    if (!post_ctx || post_ctx->offset >= post_ctx->body_len) {
        return post_data;
    }

    remaining = post_ctx->body_len - post_ctx->offset;
    if (remaining > VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE) {
        remaining = VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE;
    }

    post_data.pData = (void *)(post_ctx->body + post_ctx->offset);
    post_data.pLength = (int32_t)remaining;
    post_ctx->offset += remaining;
    return post_data;
}

static int build_multipart_body(const uint8_t *jpg_data, uint32_t jpg_len,
                                uint8_t **body_out, uint32_t *body_len_out,
                                uint32_t *header_len_out, char **boundary_out)
{
    if (!jpg_data || jpg_len == 0 || !body_out || !body_len_out ||
        !header_len_out || !boundary_out) {
        return -1;
    }

    const char *boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    *boundary_out = lisa_mem_calloc(1, strlen(boundary) + 1);
    if (!*boundary_out) {
        LOGE("Failed to allocate boundary");
        return -1;
    }
    strcpy(*boundary_out, boundary);

    uint32_t timestamp = xTaskGetTickCount();
    char filename[64];
    snprintf(filename, sizeof(filename), "photo_%u.jpg", timestamp);

    const char *header_template = "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: image/jpeg\r\n\r\n";
    const char *footer_template = "\r\n--%s--\r\n";

    int header_len = strlen(header_template) + strlen(boundary) + strlen(filename) - 4;
    int footer_len = strlen(footer_template) + strlen(boundary) - 2;
    uint32_t total_len = header_len + jpg_len + footer_len;

    *body_out = lisa_mem_calloc(1, total_len + 1);
    if (!*body_out) {
        LOGE("Failed to allocate multipart body");
        lisa_mem_free(*boundary_out);
        *boundary_out = NULL;
        return -1;
    }

    int offset = sprintf((char *)*body_out, header_template, boundary, filename);
    *header_len_out = (uint32_t)offset;

    memcpy(*body_out + offset, jpg_data, jpg_len);
    offset += jpg_len;

    int footer_written = sprintf((char *)(*body_out + offset), footer_template, boundary);
    offset += footer_written;

    *body_len_out = offset;
    LOGI("Multipart body built: %d bytes, filename: %s", *body_len_out, filename);

    return 0;
}

static char *http_client_get_multipart_headers(const char *boundary)
{
    const char *auth_token = voice_token_get();
    const char *headers = "Content-Type: multipart/form-data; boundary=%s\r\nAuthorization: Bearer %s";
    if (!auth_token) {
        LOGE("auth token is null");
        return NULL;
    }

    if (auth_header) {
        lisa_mem_free(auth_header);
        auth_header = NULL;
    }

    auth_header = lisa_mem_calloc(1, strlen(headers) + strlen(boundary) + strlen(auth_token) + 1);
    if (!auth_header) {
        LOGE("has no mem\n");
        return NULL;
    }
    sprintf(auth_header, headers, boundary, auth_token);

    return auth_header;
}

int voice_cloud_upload_jpeg_img(const uint8_t *jpeg_data, size_t jpeg_size, char **url_out)
{
    SemaphoreHandle_t lock = NULL;
    bool upload_lock_held = false;

    if (!jpeg_data || jpeg_size == 0) {
        LOGE("Invalid input parameters");
        return -1;
    }

    lock = voice_img_upload_lock_get();
    if (lock == NULL) {
        LOGE("Failed to create JPEG upload lock");
        return -1;
    }

    if (xSemaphoreTake(lock, 0) != pdTRUE) {
        LOGI("JPEG upload waits for active request");
        if (xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
            LOGE("Failed to lock JPEG upload");
            return -1;
        }
    }
    upload_lock_held = true;

    uint8_t *multipart_body = NULL;
    uint32_t multipart_len = 0;
    uint32_t multipart_header_len = 0;
    char *boundary = NULL;
    struct voice_img_upload_post_ctx *post_ctx = NULL;
    struct response_data resp_data = {0};
    int ret = -1;
    char *response_buf = NULL;
    int attempt = 0;

    LOGI("Starting JPG upload, size: %zu bytes", jpeg_size);

    struct app_datas *app_data = get_app_datas();
    if (!app_data) {
        LOGE("app_data is NULL");
        goto exit;
    }

    const char *host_suffix = "";
    if (app_data->device_mode == DEVICE_MODE_STAGING) {
        host_suffix = "staging-";
    } else if (app_data->device_mode == DEVICE_MODE_INTEGRATION) {
        host_suffix = "integration-";
    }

    if (build_multipart_body(jpeg_data, jpeg_size, &multipart_body, &multipart_len,
                             &multipart_header_len, &boundary) != 0) {
        LOGE("Failed to build multipart body");
        goto exit;
    }

    upload_headers = http_client_get_multipart_headers(boundary);
    if (!upload_headers) {
        LOGE("Failed to get headers");
        goto exit;
    }

    LOGI("Request prepared - JPEG: %zu bytes, Body: %d bytes (with headers)", jpeg_size, multipart_len);

    HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
    if (!http_param) {
        LOGE("Failed to allocate HTTPParameters");
        upload_headers = NULL;
        goto exit;
    }

    post_ctx = lisa_mem_calloc(1, sizeof(*post_ctx));
    if (!post_ctx) {
        LOGE("Failed to allocate upload stream context");
        lisa_mem_free(http_param);
        http_param = NULL;
        upload_headers = NULL;
        goto exit;
    }

    snprintf(http_param->Uri, sizeof(http_param->Uri), "http://%sapi.listenai.com%s",
             host_suffix, API_UPLOAD_PATH);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = VOICE_IMG_UPLOAD_TIMEOUT_SEC;
    http_param->pLength = multipart_len;

    for (attempt = 0; attempt < VOICE_IMG_UPLOAD_MAX_ATTEMPTS; attempt++) {
        if (attempt > 0) {
            LOGI("Retrying upload (attempt %d/%d)...", attempt + 1,
                 VOICE_IMG_UPLOAD_MAX_ATTEMPTS);
            vTaskDelay(pdMS_TO_TICKS(VOICE_IMG_UPLOAD_RETRY_DELAY_MS));
        }

        if (voice_img_upload_prime_post_ctx(post_ctx, multipart_body, multipart_len,
                                            multipart_header_len) != 0) {
            LOGE("Failed to prepare upload stream");
            lisa_mem_free(http_param);
            http_param = NULL;
            upload_headers = NULL;
            goto exit;
        }
        http_param->pData = post_ctx->head_chunk;

        int open_ret = HTTPC_open(http_param);
        if (open_ret != 0) {
            LOGE("HTTPC_open failed with code: %d", open_ret);
            if (attempt < VOICE_IMG_UPLOAD_MAX_ATTEMPTS - 1) {
                continue;
            }
            lisa_mem_free(http_param);
            http_param = NULL;
            upload_headers = NULL;
            goto exit;
        }

        s_voice_img_upload_post_ctx = post_ctx;
        int req_ret = HTTPC_request_r(http_param, http_get_headers_callback,
                                      voice_img_upload_get_post_data);
        s_voice_img_upload_post_ctx = NULL;
        if (req_ret != 0) {
            LOGE("HTTPC_request failed with code: %d, attempt: %d/%d", req_ret,
                 attempt + 1, VOICE_IMG_UPLOAD_MAX_ATTEMPTS);
            HTTPC_close(http_param);

            if (attempt < VOICE_IMG_UPLOAD_MAX_ATTEMPTS - 1) {
                continue;
            }

            lisa_mem_free(http_param);
            http_param = NULL;
            upload_headers = NULL;
            goto exit;
        }

        LOGI("HTTP request succeeded on attempt %d/%d, streamed=%u/%u bytes, chunk=%u",
             attempt + 1, VOICE_IMG_UPLOAD_MAX_ATTEMPTS, post_ctx->offset,
             multipart_len, (unsigned int)VOICE_IMG_UPLOAD_STREAM_CHUNK_SIZE);
        break;
    }

    if (attempt >= VOICE_IMG_UPLOAD_MAX_ATTEMPTS) {
        LOGE("Upload failed after %d attempts", VOICE_IMG_UPLOAD_MAX_ATTEMPTS);
        goto exit;
    }

    HTTP_CLIENT http_client = {0};
    if (HTTPC_get_request_info(http_param, &http_client) != 0) {
        LOGE("HTTPC_get_request_info failed");
        HTTPC_close(http_param);
        lisa_mem_free(http_param);
        upload_headers = NULL;
        goto exit;
    }

    upload_headers = NULL;

    if (http_client.TotalResponseBodyLength > 0) {
        unsigned int received = 0;
        unsigned int readsize = 0;
        response_buf = lisa_mem_calloc(1, http_client.TotalResponseBodyLength + 1);
        if (!response_buf) {
            LOGE("Failed to allocate response buffer");
            HTTPC_close(http_param);
            lisa_mem_free(http_param);
            goto exit;
        }

        do {
            if (HTTPC_read(http_param, response_buf + readsize, 4096, (void *)&received) != 0) {
                if (received > 0) readsize += received;
                break;
            } else {
                readsize += received;
            }
        } while (readsize < http_client.TotalResponseBodyLength);

        LOGI("Response received: %d bytes", readsize);
        LOGI("Response body: %.*s", readsize, response_buf);

        cJSON *root = cJSON_ParseWithLength(response_buf, readsize);
        if (root) {
            cJSON *status_code = cJSON_GetObjectItem(root, "statusCode");
            if (cJSON_IsNumber(status_code)) {
                resp_data.status_code = status_code->valueint;
                LOGI("Status code: %d", resp_data.status_code);
            }

            if (resp_data.status_code == 0) {
                cJSON *data_obj = cJSON_GetObjectItem(root, "data");
                if (cJSON_IsObject(data_obj)) {
                    cJSON *url = cJSON_GetObjectItem(data_obj, "url");
                    if (cJSON_IsString(url) && url->valuestring) {
                        int url_len = strlen(url->valuestring);
                        resp_data.url = lisa_mem_calloc(1, url_len + 1);
                        if (resp_data.url) {
                            strcpy(resp_data.url, url->valuestring);
                            LOGI("Upload successful! URL: %s", resp_data.url);
                        }
                    }
                }
            }

            cJSON_Delete(root);
        }
    }

    HTTPC_close(http_param);
    lisa_mem_free(http_param);

    if (resp_data.status_code == 0 && resp_data.url) {
        if (url_out) {
            *url_out = resp_data.url;
            resp_data.url = NULL;
        }
        ret = 0;
    } else {
        LOGE("Upload failed, status_code: %d", resp_data.status_code);
        ret = -1;
    }

exit:
    s_voice_img_upload_post_ctx = NULL;
    upload_headers = NULL;

    if (auth_header) {
        lisa_mem_free(auth_header);
        auth_header = NULL;
    }

    if (multipart_body) {
        lisa_mem_free(multipart_body);
    }
    if (post_ctx) {
        lisa_mem_free(post_ctx);
    }
    if (boundary) {
        lisa_mem_free(boundary);
    }
    if (response_buf) {
        lisa_mem_free(response_buf);
    }
    if (resp_data.url) {
        lisa_mem_free(resp_data.url);
    }
    if (upload_lock_held) {
        xSemaphoreGive(lock);
    }

    return ret;
}

void voice_cloud_jpeg_img_url_free(void *url)
{
    if (url) {
        lisa_mem_free(url);
    }
}
