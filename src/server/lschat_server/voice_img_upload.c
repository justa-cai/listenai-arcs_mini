#include "lisa_log.h"
#include "HTTPCUsr_api.h"
#include "cJSON.h"
#include "lisa_mem.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#define TAG "img_upload"

#define API_HOST        "http://api.listenai.com"
#define API_UPLOAD_PATH "/v1/device/assets"
#define API_UPLOAD_URL  API_HOST API_UPLOAD_PATH

static char *auth_header = NULL;
static volatile char *upload_headers = NULL;

extern uint8_t *voice_token_get(void);

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

static int build_multipart_body(const uint8_t *jpg_data, uint32_t jpg_len,
                                uint8_t **body_out, uint32_t *body_len_out,
                                char **boundary_out)
{
    if (!jpg_data || jpg_len == 0 || !body_out || !body_len_out || !boundary_out) {
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
    if (!jpeg_data || jpeg_size == 0) {
        LOGE("Invalid input parameters");
        return -1;
    }

    uint8_t *multipart_body = NULL;
    uint32_t multipart_len = 0;
    char *boundary = NULL;
    struct response_data resp_data = {0};
    int ret = -1;
    char *response_buf = NULL;
    int retry_count = 0;
    const int MAX_RETRIES = 1;

    LOGI("Starting JPG upload, size: %zu bytes", jpeg_size);

    if (build_multipart_body(jpeg_data, jpeg_size, &multipart_body, &multipart_len, &boundary) != 0) {
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

    strcpy(http_param->Uri, API_UPLOAD_URL);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = 10;
    http_param->pData = multipart_body;
    http_param->pLength = multipart_len;

    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++) {
        if (retry_count > 0) {
            LOGI("Retrying upload (attempt %d/%d)...", retry_count + 1, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        int open_ret = HTTPC_open(http_param);
        if (open_ret != 0) {
            LOGE("HTTPC_open failed with code: %d", open_ret);
            if (retry_count < MAX_RETRIES - 1) {
                continue;
            }
            lisa_mem_free(http_param);
            upload_headers = NULL;
            goto exit;
        }

        int req_ret = HTTPC_request(http_param, http_get_headers_callback);
        if (req_ret != 0) {
            LOGE("HTTPC_request failed with code: %d, retry: %d/%d", req_ret, retry_count + 1, MAX_RETRIES);
            HTTPC_close(http_param);

            if (retry_count < MAX_RETRIES - 1) {
                continue;
            }

            lisa_mem_free(http_param);
            upload_headers = NULL;
            goto exit;
        }

        LOGI("HTTP request succeeded on attempt %d", retry_count + 1);
        break;
    }

    if (retry_count >= MAX_RETRIES) {
        LOGE("Upload failed after %d retries", MAX_RETRIES);
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
    upload_headers = NULL;

    if (multipart_body) {
        lisa_mem_free(multipart_body);
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

    return ret;
}

void voice_cloud_jpeg_img_url_free(void *url)
{
    if (url) {
        lisa_mem_free(url);
    }
}
