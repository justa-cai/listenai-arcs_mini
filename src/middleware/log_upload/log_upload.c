#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "HTTPCUsr_api.h"
#include "lisa_mem.h"
#include "lisa_thread.h"

#include "voice_msg.h"
#define TAG "log_upload"
#include "lisa_log.h"
#include "log_buffer.h"
#include "log_upload.h"

#define LOG_UPLOAD_TIMEOUT_SEC 10
#define LOG_UPLOAD_HEADER_TEXT "Content-Type: text/plain; charset=utf-8"
#define LOG_UPLOAD_READ_BUF_SIZE 256U
#define LOG_UPLOAD_RESPONSE_PREVIEW_MAX 255U
#define LOG_UPLOAD_TASK_STACK_SIZE 8192U
#define LOG_UPLOAD_MAX_LOG_BYTES LOG_BUFFER_CAPACITY

static volatile bool g_log_upload_running = false;
static log_upload_state_t g_log_upload_state;

struct log_upload_response {
    uint32_t body_len;
    char preview[LOG_UPLOAD_RESPONSE_PREVIEW_MAX + 1];
};

struct log_upload_task_ctx {
    log_upload_complete_cb_t complete_cb;
    void *complete_cb_arg;
    char upload_url[HTTP_CLIENT_MAX_URL_LENGTH];
};

static void *log_upload_headers_callback(void)
{
    return (void *)LOG_UPLOAD_HEADER_TEXT;
}

static void log_upload_response_reset(struct log_upload_response *response)
{
    if (!response) {
        return;
    }

    response->body_len = 0;
    response->preview[0] = '\0';
}

static void log_upload_response_append(struct log_upload_response *response,
                                       const void *buf,
                                       uint32_t len)
{
    uint32_t copy_len;

    if (!response || !buf || len == 0) {
        return;
    }

    copy_len = LOG_UPLOAD_RESPONSE_PREVIEW_MAX - response->body_len;
    if (copy_len > len) {
        copy_len = len;
    }

    if (copy_len > 0) {
        memcpy(response->preview + response->body_len, buf, copy_len);
        response->body_len += copy_len;
        response->preview[response->body_len] = '\0';
    }
}

static int log_upload_build_text_body(const struct log_buffer_snapshot *snapshot,
                                      uint8_t **body_out,
                                      uint32_t *body_len_out)
{
    uint8_t *body;
    const uint8_t *first_data;
    const uint8_t *second_data;
    uint32_t first_size;
    uint32_t second_size;
    uint32_t total_len;
    uint32_t skip_bytes = 0;
    uint32_t copied = 0;

    if (!snapshot || !body_out || !body_len_out) {
        return -EINVAL;
    }

    *body_out = NULL;
    *body_len_out = 0;
    if (snapshot->valid_bytes == 0) {
        return 0;
    }

    first_data = snapshot->first.data;
    first_size = snapshot->first.size;
    second_data = snapshot->second.data;
    second_size = snapshot->second.size;
    total_len = first_size + second_size;

    if (total_len > LOG_UPLOAD_MAX_LOG_BYTES) {
        skip_bytes = total_len - LOG_UPLOAD_MAX_LOG_BYTES;
        if (skip_bytes >= first_size) {
            skip_bytes -= first_size;
            first_data = second_data ? second_data + skip_bytes : NULL;
            first_size = second_size - skip_bytes;
            second_data = NULL;
            second_size = 0;
        } else {
            first_data += skip_bytes;
            first_size -= skip_bytes;
        }
        total_len = LOG_UPLOAD_MAX_LOG_BYTES;
    }

    body = lisa_mem_alloc(total_len + 1);
    if (!body) {
        return -ENOMEM;
    }

    if (first_size > 0 && first_data) {
        memcpy(body, first_data, first_size);
        copied += first_size;
    }
    if (second_size > 0 && second_data) {
        memcpy(body + copied, second_data, second_size);
        copied += second_size;
    }

    body[copied] = '\0';
    *body_out = body;
    *body_len_out = copied;
    return 0;
}

static bool log_upload_try_mark_running(void)
{
    bool marked = false;

    taskENTER_CRITICAL();
    if (!g_log_upload_running) {
        g_log_upload_running = true;
        marked = true;
    }
    taskEXIT_CRITICAL();

    return marked;
}

static void log_upload_clear_running(void)
{
    taskENTER_CRITICAL();
    g_log_upload_running = false;
    taskEXIT_CRITICAL();
}

static void log_upload_publish_state_msg(uint32_t msg_id)
{
    log_upload_state_t snapshot;

    taskENTER_CRITICAL();
    snapshot = g_log_upload_state;
    taskEXIT_CRITICAL();

    voice_msg_pub(msg_id, &snapshot, sizeof(snapshot));
}

static void log_upload_set_state(log_upload_state_e state, int result)
{
    taskENTER_CRITICAL();
    g_log_upload_state.state = state;
    g_log_upload_state.result = result;
    taskEXIT_CRITICAL();

    switch (state) {
    case LOG_UPLOAD_STATE_STARTING:
        log_upload_publish_state_msg(VOICE_MSG_LOG_UPLOAD_STARTING);
        break;
    case LOG_UPLOAD_STATE_UPLOADING:
        log_upload_publish_state_msg(VOICE_MSG_LOG_UPLOAD_UPLOADING);
        break;
    case LOG_UPLOAD_STATE_SUCCESSED:
        log_upload_publish_state_msg(VOICE_MSG_LOG_UPLOAD_SUCCESSED);
        break;
    case LOG_UPLOAD_STATE_FAILED:
        log_upload_publish_state_msg(VOICE_MSG_LOG_UPLOAD_FAILED);
        break;
    default:
        break;
    }
}

static void log_upload_call_complete(const struct log_upload_task_ctx *ctx)
{
    log_upload_state_t snapshot;

    if (!ctx || !ctx->complete_cb) {
        return;
    }

    if (log_upload_get_state_snapshot(&snapshot) != 0) {
        return;
    }

    ctx->complete_cb(&snapshot, ctx->complete_cb_arg);
}

static int log_upload_read_response(HTTPParameters *http_param,
                                    uint32_t expected_len,
                                    struct log_upload_response *response)
{
    uint8_t read_buf[LOG_UPLOAD_READ_BUF_SIZE];
    UINT32 received = 0;
    uint32_t total_read = 0;
    int read_ret = 0;

    if (!http_param || !response) {
        return -EINVAL;
    }

    log_upload_response_reset(response);
    if (expected_len == 0) {
        return 0;
    }

    do {
        uint32_t to_read = expected_len - total_read;
        if (to_read > LOG_UPLOAD_READ_BUF_SIZE) {
            to_read = LOG_UPLOAD_READ_BUF_SIZE;
        }

        read_ret = HTTPC_read(http_param, read_buf, to_read, (void *)&received);
        if (received > 0) {
            total_read += received;
            log_upload_response_append(response, read_buf, received);
        }
    } while (read_ret == 0 && total_read < expected_len);

    if (total_read != expected_len) {
        LISA_LOGE(TAG, "upload response read incomplete: got=%u expect=%u", total_read, expected_len);
        return -EIO;
    }

    return 0;
}

static int log_upload_send_request(const char *upload_url, const uint8_t *body, uint32_t body_len)
{
    struct log_upload_response response = {0};
    HTTPParameters *http_param;
    HTTP_CLIENT http_client = {0};
    int ret = -EIO;
    bool opened = false;

    if (!upload_url || upload_url[0] == '\0' || !body || body_len == 0) {
        return -EINVAL;
    }

    http_param = lisa_mem_calloc(1, sizeof(*http_param));
    if (!http_param) {
        return -ENOMEM;
    }

    if (strlen(upload_url) >= sizeof(http_param->Uri)) {
        lisa_mem_free(http_param);
        return -ENOSPC;
    }

    strcpy(http_param->Uri, upload_url);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = LOG_UPLOAD_TIMEOUT_SEC;
    http_param->pData = (void *)body;
    http_param->pLength = body_len;

    LISA_LOGI(TAG, "log upload start url=%s body_len=%u", upload_url, body_len);

    if (HTTPC_open(http_param) != 0) {
        LISA_LOGE(TAG, "HTTPC_open failed");
        goto exit;
    }
    opened = true;

    ret = HTTPC_request(http_param, log_upload_headers_callback);
    if (ret != 0) {
        LISA_LOGE(TAG, "HTTPC_request failed (%d)", ret);
        ret = -EIO;
        goto exit;
    }

    if (HTTPC_get_request_info(http_param, &http_client) != 0) {
        LISA_LOGE(TAG, "HTTPC_get_request_info failed");
        ret = -EIO;
        goto exit;
    }

    ret = log_upload_read_response(http_param, http_client.TotalResponseBodyLength, &response);
    if (ret != 0) {
        LISA_LOGE(TAG, "failed to read upload response");
        goto exit;
    }

    LISA_LOGI(TAG, "upload response status=%u body_len=%u",
              http_client.HTTPStatusCode, http_client.TotalResponseBodyLength);
    if (response.preview[0] != '\0') {
        LISA_LOGI(TAG, "upload response body: %s", response.preview);
    } else {
        LISA_LOGI(TAG, "upload response body: <empty>");
    }

    if (http_client.HTTPStatusCode < 200 || http_client.HTTPStatusCode >= 300) {
        LISA_LOGE(TAG, "upload rejected by server, status=%u", http_client.HTTPStatusCode);
        ret = (int)http_client.HTTPStatusCode;
        goto exit;
    }

    ret = 0;

 exit:
    if (opened) {
        HTTPC_close(http_param);
    }
    lisa_mem_free(http_param);
    return ret;
}

static int log_upload_perform(const char *upload_url)
{
    struct log_buffer_snapshot snapshot = {0};
    uint8_t *body = NULL;
    uint32_t body_len = 0;
    bool snapshot_acquired = false;
    int ret;

    if (!upload_url || upload_url[0] == '\0') {
        return -EINVAL;
    }

    ret = log_buffer_snapshot_acquire(&snapshot);
    if (ret != 0) {
        return ret;
    }
    snapshot_acquired = true;

    ret = log_upload_build_text_body(&snapshot, &body, &body_len);
    if (ret != 0) {
        goto exit;
    }
    if (body_len == 0) {
        ret = -ENODATA;
        goto exit;
    }

    ret = log_upload_send_request(upload_url, body, body_len);
    if (ret == 0) {
        log_buffer_reset();
    }

 exit:
    if (snapshot_acquired) {
        log_buffer_snapshot_release();
    }
    if (body) {
        lisa_mem_free(body);
    }
    return ret;
}

static void log_upload_task(void *arg)
{
    struct log_upload_task_ctx *ctx = (struct log_upload_task_ctx *)arg;
    int ret;

    log_upload_set_state(LOG_UPLOAD_STATE_UPLOADING, 0);
    ret = log_upload_perform(ctx ? ctx->upload_url : NULL);
    if (ret == 0) {
        LISA_LOGI(TAG, "log upload finished");
        log_upload_set_state(LOG_UPLOAD_STATE_SUCCESSED, 0);
    } else {
        LISA_LOGE(TAG, "log upload failed (%d)", ret);
        log_upload_set_state(LOG_UPLOAD_STATE_FAILED, ret);
    }

    log_upload_clear_running();
    log_upload_call_complete(ctx);
    if (ctx) {
        lisa_mem_free(ctx);
    }
    lisa_thread_delete(NULL);
}

int log_upload_trigger(void)
{
    return log_upload_trigger_with_url(NULL, NULL, NULL);
}

int log_upload_trigger_with_url(const char *upload_url, log_upload_complete_cb_t complete_cb, void *arg)
{
    lisa_thread_attr_t attr = {
        .name = (uint8_t *)"log_upload",
        .stack_size = LOG_UPLOAD_TASK_STACK_SIZE,
        .priority = LISA_OS_PRIORITY_LOW,
    };
    struct log_upload_task_ctx *ctx = NULL;

    if (xPortIsInsideInterrupt()) {
        return -EPERM;
    }

    if (!upload_url || upload_url[0] == '\0') {
        return -EINVAL;
    }

    if (strlen(upload_url) >= HTTP_CLIENT_MAX_URL_LENGTH) {
        return -ENOSPC;
    }

    if (!log_upload_try_mark_running()) {
        return -EBUSY;
    }

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        log_upload_clear_running();
        log_upload_set_state(LOG_UPLOAD_STATE_FAILED, -ENOMEM);
        return -ENOMEM;
    }

    ctx->complete_cb = complete_cb;
    ctx->complete_cb_arg = arg;
    strcpy(ctx->upload_url, upload_url);

    log_upload_set_state(LOG_UPLOAD_STATE_STARTING, 0);
    voice_msg_pub(VOICE_MSG_CLOUD_SESSION_INTERRUPT, NULL, 0);

    if (lisa_thread_create(&attr, log_upload_task, ctx) == NULL) {
        lisa_mem_free(ctx);
        log_upload_clear_running();
        log_upload_set_state(LOG_UPLOAD_STATE_FAILED, -ENOMEM);
        return -ENOMEM;
    }

    LISA_LOGI(TAG, "log upload scheduled");
    return 0;
}

int log_upload_get_state_snapshot(log_upload_state_t *state)
{
    if (!state) {
        return -EINVAL;
    }

    taskENTER_CRITICAL();
    *state = g_log_upload_state;
    taskEXIT_CRITICAL();

    return 0;
}

int log_upload_is_running(void)
{
    return g_log_upload_running ? 1 : 0;
}
