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

#define LOG_UPLOAD_TIMEOUT_SEC 60U
#define LOG_UPLOAD_MAX_ATTEMPTS 2U
#define LOG_UPLOAD_RETRY_DELAY_MS 2000U
#define LOG_UPLOAD_HEADER_TEXT "Content-Type: text/plain; charset=utf-8"
#define LOG_UPLOAD_READ_BUF_SIZE 256U
#define LOG_UPLOAD_RESPONSE_PREVIEW_MAX 255U
#define LOG_UPLOAD_TASK_STACK_SIZE 8192U
#define LOG_UPLOAD_MAX_LOG_BYTES LOG_BUFFER_CAPACITY
#define LOG_UPLOAD_STREAM_CHUNK_SIZE HTTP_CLIENT_BUFFER_SIZE
#define LOG_UPLOAD_UI_READY_DELAY_MS 300U

static volatile bool s_log_upload_running = false;
static log_upload_state_t s_log_upload_state;

struct log_upload_body_view {
    const uint8_t *first_data;
    uint32_t first_size;
    const uint8_t *second_data;
    uint32_t second_size;
    uint32_t total_len;
};

struct log_upload_response {
    uint32_t preview_len;
    char preview[LOG_UPLOAD_RESPONSE_PREVIEW_MAX + 1];
};

struct log_upload_task_ctx {
    log_upload_complete_cb_t complete_cb;
    void *complete_cb_arg;
    char upload_url[HTTP_CLIENT_MAX_URL_LENGTH];
};

/*
 * 这里仍然是一个普通的带 Content-Length 的 POST。
 * “分块”只是设备本地按小块把 body 交给底层发送，避免一次性准备大块连续内存。
 */
struct log_upload_post_ctx {
    const uint8_t *first_data;
    uint32_t first_size;
    uint32_t first_offset;
    const uint8_t *second_data;
    uint32_t second_size;
    uint32_t second_offset;
    char head_chunk[LOG_UPLOAD_STREAM_CHUNK_SIZE + 1];
};

struct log_upload_request {
    HTTPParameters *http_param;
    struct log_upload_post_ctx *post_ctx;
    HTTP_CLIENT http_client;
    bool opened;
};

/*
 * HTTPC_request_r() 的取数回调没有用户指针。
 * 当前上传本身就是串行的，所以这里用一个全局上下文承接本次请求即可。
 */
static struct log_upload_post_ctx *s_log_upload_post_ctx;

/* 提供固定 HTTP 头，目前只声明上传内容是 UTF-8 文本。 */
static void *log_upload_headers_callback(void)
{
    return (void *)LOG_UPLOAD_HEADER_TEXT;
}

/* 原子地把上传状态从“空闲”切到“运行中”，避免并发上传。 */
static bool log_upload_try_mark_running(void)
{
    bool marked = false;

    taskENTER_CRITICAL();
    if (!s_log_upload_running) {
        s_log_upload_running = true;
        marked = true;
    }
    taskEXIT_CRITICAL();

    return marked;
}

/* 清除运行标记，允许下一次上传任务进入。 */
static void log_upload_clear_running(void)
{
    taskENTER_CRITICAL();
    s_log_upload_running = false;
    taskEXIT_CRITICAL();
}

/* 根据当前上传状态向语音业务广播状态事件。 */
static void log_upload_publish_state_event(void)
{
    uint32_t msg_id;
    log_upload_state_t snapshot;

    taskENTER_CRITICAL();
    snapshot = s_log_upload_state;
    taskEXIT_CRITICAL();

    switch (snapshot.state) {
    case LOG_UPLOAD_STATE_STARTING:
        msg_id = VOICE_MSG_LOG_UPLOAD_STARTING;
        break;
    case LOG_UPLOAD_STATE_UPLOADING:
        msg_id = VOICE_MSG_LOG_UPLOAD_UPLOADING;
        break;
    case LOG_UPLOAD_STATE_SUCCESSED:
        msg_id = VOICE_MSG_LOG_UPLOAD_SUCCESSED;
        break;
    case LOG_UPLOAD_STATE_FAILED:
        msg_id = VOICE_MSG_LOG_UPLOAD_FAILED;
        break;
    default:
        return;
    }

    voice_msg_pub(msg_id, &snapshot, sizeof(snapshot));
}

/* 更新内部状态快照，并同步通知上层状态变化。 */
static void log_upload_notify_state(log_upload_state_e state, int result)
{
    taskENTER_CRITICAL();
    s_log_upload_state.state = state;
    s_log_upload_state.result = result;
    taskEXIT_CRITICAL();

    log_upload_publish_state_event();
}

/* 根据环形缓冲区快照构造本次上传要发送的两段 body 视图。 */
static int log_upload_build_body_view(const struct log_buffer_snapshot *snapshot,
                                      struct log_upload_body_view *body_view)
{
    const uint8_t *first_data;
    const uint8_t *second_data;
    uint32_t first_size;
    uint32_t second_size;
    uint32_t total_len;
    uint32_t skip_bytes = 0;

    if (!snapshot || !body_view) {
        return -EINVAL;
    }

    memset(body_view, 0, sizeof(*body_view));
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

    body_view->first_data = first_data;
    body_view->first_size = first_size;
    body_view->second_data = second_data;
    body_view->second_size = second_size;
    body_view->total_len = total_len;
    return 0;
}

/* 预装第一块 body 数据，供 HTTPC_open/HTTPC_request_r 立刻开始发送。 */
static void log_upload_prime_post_ctx(struct log_upload_post_ctx *post_ctx,
                                      const struct log_upload_body_view *body_view)
{
    uint32_t remaining;
    uint32_t copied = 0;

    if (!post_ctx || !body_view) {
        return;
    }

    memset(post_ctx, 0, sizeof(*post_ctx));
    post_ctx->first_data = body_view->first_data;
    post_ctx->first_size = body_view->first_size;
    post_ctx->second_data = body_view->second_data;
    post_ctx->second_size = body_view->second_size;

    remaining = body_view->total_len;
    if (remaining > LOG_UPLOAD_STREAM_CHUNK_SIZE) {
        remaining = LOG_UPLOAD_STREAM_CHUNK_SIZE;
    }

    if (remaining > 0 && post_ctx->first_data && post_ctx->first_size > 0) {
        uint32_t first_copy = remaining;

        if (first_copy > post_ctx->first_size) {
            first_copy = post_ctx->first_size;
        }
        memcpy(post_ctx->head_chunk, post_ctx->first_data, first_copy);
        post_ctx->first_offset = first_copy;
        copied += first_copy;
        remaining -= first_copy;
    }

    if (remaining > 0 && post_ctx->second_data && post_ctx->second_size > 0) {
        uint32_t second_copy = remaining;

        if (second_copy > post_ctx->second_size) {
            second_copy = post_ctx->second_size;
        }
        memcpy(post_ctx->head_chunk + copied, post_ctx->second_data, second_copy);
        post_ctx->second_offset = second_copy;
        copied += second_copy;
    }

    post_ctx->head_chunk[copied] = '\0';
}

/* 统计本次请求已经交给 HTTP 栈的 body 字节数。 */
static uint32_t log_upload_post_ctx_delivered_len(const struct log_upload_post_ctx *post_ctx)
{
    if (!post_ctx) {
        return 0;
    }

    return post_ctx->first_offset + post_ctx->second_offset;
}

/* 提供后续 body 分片给 HTTPC_request_r。 */
static PostData log_upload_get_post_data(void)
{
    PostData post_data = {.pData = NULL, .pLength = 0};
    struct log_upload_post_ctx *post_ctx = s_log_upload_post_ctx;
    uint32_t remaining;

    if (!post_ctx) {
        return post_data;
    }

    if (post_ctx->first_data && post_ctx->first_offset < post_ctx->first_size) {
        remaining = post_ctx->first_size - post_ctx->first_offset;
        if (remaining > LOG_UPLOAD_STREAM_CHUNK_SIZE) {
            remaining = LOG_UPLOAD_STREAM_CHUNK_SIZE;
        }
        post_data.pData = (void *)(post_ctx->first_data + post_ctx->first_offset);
        post_data.pLength = (int32_t)remaining;
        post_ctx->first_offset += remaining;
        return post_data;
    }

    if (post_ctx->second_data && post_ctx->second_offset < post_ctx->second_size) {
        remaining = post_ctx->second_size - post_ctx->second_offset;
        if (remaining > LOG_UPLOAD_STREAM_CHUNK_SIZE) {
            remaining = LOG_UPLOAD_STREAM_CHUNK_SIZE;
        }
        post_data.pData = (void *)(post_ctx->second_data + post_ctx->second_offset);
        post_data.pLength = (int32_t)remaining;
        post_ctx->second_offset += remaining;
    }

    return post_data;
}

/* 读取完整 HTTP 响应体，并截取一小段内容用于日志打印。 */
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

    response->preview_len = 0;
    response->preview[0] = '\0';
    if (expected_len == 0) {
        return 0;
    }

    do {
        uint32_t to_read = expected_len - total_read;

        if (to_read > LOG_UPLOAD_READ_BUF_SIZE) {
            to_read = LOG_UPLOAD_READ_BUF_SIZE;
        }

        received = 0;
        read_ret = HTTPC_read(http_param, read_buf, to_read, (void *)&received);
        if (received > 0) {
            uint32_t copy_len = LOG_UPLOAD_RESPONSE_PREVIEW_MAX - response->preview_len;

            total_read += received;
            if (copy_len > received) {
                copy_len = received;
            }
            if (copy_len > 0) {
                memcpy(response->preview + response->preview_len, read_buf, copy_len);
                response->preview_len += copy_len;
                response->preview[response->preview_len] = '\0';
            }
        }
    } while (read_ret == 0 && total_read < expected_len);

    /*
     * HTTPC_read() 可能会在最后一批数据已经拷贝完成后再返回 HTTP_CLIENT_EOS。
     * 只要我们已经读满服务端声明的长度，就按成功处理，避免误判失败。
     */
    if (total_read == expected_len) {
        return 0;
    }

    if (read_ret != 0) {
        LISA_LOGE(TAG, "Upload response read failed: ret=%d got=%u expect=%u",
                  read_ret, total_read, expected_len);
        return -read_ret;
    }

    LISA_LOGE(TAG, "Upload response read incomplete: got=%u expect=%u", total_read, expected_len);
    return -EIO;
}

/* 为一次上传请求分配 HTTP 参数和分片发送上下文。 */
static int log_upload_request_init(struct log_upload_request *request,
                                   const char *upload_url,
                                   const struct log_upload_body_view *body_view)
{
    uint32_t body_len;

    if (!request || !upload_url || upload_url[0] == '\0' || !body_view || body_view->total_len == 0) {
        return -EINVAL;
    }

    body_len = body_view->total_len;
    memset(request, 0, sizeof(*request));

    request->http_param = lisa_mem_calloc(1, sizeof(*request->http_param));
    if (!request->http_param) {
        return -ENOMEM;
    }

    request->post_ctx = lisa_mem_calloc(1, sizeof(*request->post_ctx));
    if (!request->post_ctx) {
        lisa_mem_free(request->http_param);
        request->http_param = NULL;
        return -ENOMEM;
    }

    if (strlen(upload_url) >= sizeof(request->http_param->Uri)) {
        return -ENOSPC;
    }

    strcpy(request->http_param->Uri, upload_url);
    request->http_param->HttpVerb = VerbPost;
    request->http_param->nTimeout = LOG_UPLOAD_TIMEOUT_SEC;
    request->http_param->pLength = body_len;

    log_upload_prime_post_ctx(request->post_ctx, body_view);
    request->http_param->pData = request->post_ctx->head_chunk;
    return 0;
}

/* 用当前日志快照执行一次真正的 HTTP 上传。 */
static int log_upload_send_request(const char *upload_url, const struct log_upload_body_view *body_view)
{
    struct log_upload_response response = {0};
    struct log_upload_request request = {0};
    TickType_t start_tick;
    TickType_t elapsed_tick;
    uint32_t body_len;
    uint32_t attempt;
    int ret;

    ret = log_upload_request_init(&request, upload_url, body_view);
    if (ret != 0) {
        goto exit;
    }

    body_len = body_view->total_len;
    for (attempt = 1U; attempt <= LOG_UPLOAD_MAX_ATTEMPTS; attempt++) {
        log_upload_prime_post_ctx(request.post_ctx, body_view);
        LISA_LOGI(TAG,
                  "Start upload: attempt=%u/%u url=%s body_len=%u timeout=%us first=%u second=%u chunk=%u",
                  attempt, LOG_UPLOAD_MAX_ATTEMPTS, upload_url, body_len,
                  (unsigned int)request.http_param->nTimeout, body_view->first_size,
                  body_view->second_size, (unsigned int)LOG_UPLOAD_STREAM_CHUNK_SIZE);

        ret = HTTPC_open(request.http_param);
        if (ret != 0) {
            LISA_LOGE(TAG, "HTTPC_open failed: ret=%d attempt=%u/%u", ret, attempt,
                      LOG_UPLOAD_MAX_ATTEMPTS);
        } else {
            request.opened = true;
            start_tick = xTaskGetTickCount();
            s_log_upload_post_ctx = request.post_ctx;
            ret = HTTPC_request_r(request.http_param, log_upload_headers_callback,
                                  log_upload_get_post_data);
            s_log_upload_post_ctx = NULL;
            elapsed_tick = xTaskGetTickCount() - start_tick;
            if (ret != 0) {
                LISA_LOGE(TAG,
                          "HTTPC_request failed: ret=%d body_len=%u elapsed=%ums attempt=%u/%u",
                          ret, body_len, (unsigned int)pdTICKS_TO_MS(elapsed_tick), attempt,
                          LOG_UPLOAD_MAX_ATTEMPTS);
            }
        }

        if (ret == 0) {
            break;
        }

        if (request.opened) {
            HTTPC_close(request.http_param);
            request.opened = false;
        }

        if (attempt < LOG_UPLOAD_MAX_ATTEMPTS) {
            LISA_LOGW(TAG, "Retrying upload after %ums", LOG_UPLOAD_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(LOG_UPLOAD_RETRY_DELAY_MS));
        }
    }

    if (ret != 0) {
        ret = -ret;
        goto exit;
    }

    ret = HTTPC_get_request_info(request.http_param, &request.http_client);
    if (ret != 0) {
        LISA_LOGE(TAG, "HTTPC_get_request_info failed: ret=%d body_len=%u elapsed=%ums",
                  ret, body_len, (unsigned int)pdTICKS_TO_MS(elapsed_tick));
        ret = -ret;
        goto exit;
    }

    ret = log_upload_read_response(request.http_param,
                                   request.http_client.TotalResponseBodyLength,
                                   &response);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to read upload response: ret=%d body_len=%u elapsed=%ums",
                  ret, body_len, (unsigned int)pdTICKS_TO_MS(elapsed_tick));
        goto exit;
    }

    LISA_LOGI(TAG,
              "Upload response: status=%u resp_body_len=%u delivered_body_len=%u expect_body_len=%u elapsed=%ums",
              request.http_client.HTTPStatusCode,
              request.http_client.TotalResponseBodyLength,
              log_upload_post_ctx_delivered_len(request.post_ctx),
              body_len,
              (unsigned int)pdTICKS_TO_MS(elapsed_tick));
    if (response.preview[0] != '\0') {
        LISA_LOGI(TAG, "Upload response body: %s", response.preview);
    } else {
        LISA_LOGI(TAG, "Upload response body: <empty>");
    }

    if (request.http_client.HTTPStatusCode < 200 || request.http_client.HTTPStatusCode >= 300) {
        LISA_LOGE(TAG, "Upload rejected by server, status=%u", request.http_client.HTTPStatusCode);
        ret = (int)request.http_client.HTTPStatusCode;
        goto exit;
    }

    if (log_upload_post_ctx_delivered_len(request.post_ctx) != body_len) {
        LISA_LOGE(TAG, "Upload body delivery mismatch: delivered=%u expect=%u",
                  log_upload_post_ctx_delivered_len(request.post_ctx), body_len);
        ret = -EIO;
        goto exit;
    }

    ret = 0;

exit:
    s_log_upload_post_ctx = NULL;
    if (request.opened && request.http_param) {
        HTTPC_close(request.http_param);
    }
    if (request.post_ctx) {
        lisa_mem_free(request.post_ctx);
    }
    if (request.http_param) {
        lisa_mem_free(request.http_param);
    }
    return ret;
}

/* 组织一次完整上传：拿快照、发送、成功后清空缓冲区。 */
static int log_upload_perform(const char *upload_url)
{
    struct log_buffer_snapshot snapshot = {0};
    struct log_upload_body_view body_view = {0};
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

    ret = log_upload_build_body_view(&snapshot, &body_view);
    if (ret != 0) {
        goto exit;
    }
    if (body_view.total_len == 0) {
        ret = -ENODATA;
        goto exit;
    }

    LISA_LOGI(TAG, "Prepared upload body: total=%u first=%u second=%u",
              body_view.total_len, body_view.first_size, body_view.second_size);

    ret = log_upload_send_request(upload_url, &body_view);
    if (ret == 0) {
        log_buffer_reset();
        LISA_LOGI(TAG, "Upload succeeded, log buffer cleared");
    }

exit:
    if (snapshot_acquired) {
        log_buffer_snapshot_release();
    }
    return ret;
}

/* 上传线程入口，负责驱动状态机并在结束时通知回调。 */
static void log_upload_task(void *arg)
{
    struct log_upload_task_ctx *ctx = (struct log_upload_task_ctx *)arg;
    int ret;

    /* 先让 STARTING 页面完成导航和首帧刷新，再打断云会话并抓取日志。 */
    vTaskDelay(pdMS_TO_TICKS(LOG_UPLOAD_UI_READY_DELAY_MS));
    log_upload_notify_state(LOG_UPLOAD_STATE_UPLOADING, 0);
    ret = log_upload_perform(ctx ? ctx->upload_url : NULL);
    if (ret == 0) {
        LISA_LOGI(TAG, "Log upload finished");
        log_upload_notify_state(LOG_UPLOAD_STATE_SUCCESSED, 0);
    } else {
        LISA_LOGE(TAG, "Log upload failed (%d)", ret);
        log_upload_notify_state(LOG_UPLOAD_STATE_FAILED, ret);
    }

    log_upload_clear_running();
    if (ctx && ctx->complete_cb) {
        log_upload_state_t snapshot;

        if (log_upload_get_state_snapshot(&snapshot) == 0) {
            ctx->complete_cb(&snapshot, ctx->complete_cb_arg);
        }
    }
    if (ctx) {
        lisa_mem_free(ctx);
    }
    lisa_thread_delete(NULL);
}

/* 对外接口：启动一次异步日志上传，并在必要时通过回调返回最终结果。 */
int log_upload_trigger(const char *upload_url, log_upload_complete_cb_t complete_cb, void *arg)
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
        log_upload_notify_state(LOG_UPLOAD_STATE_FAILED, -ENOMEM);
        return -ENOMEM;
    }

    ctx->complete_cb = complete_cb;
    ctx->complete_cb_arg = arg;
    strcpy(ctx->upload_url, upload_url);

    log_upload_notify_state(LOG_UPLOAD_STATE_STARTING, 0);

    if (lisa_thread_create(&attr, log_upload_task, ctx) == NULL) {
        lisa_mem_free(ctx);
        log_upload_clear_running();
        log_upload_notify_state(LOG_UPLOAD_STATE_FAILED, -ENOMEM);
        return -ENOMEM;
    }

    LISA_LOGI(TAG, "Log upload scheduled");
    return 0;
}

/* 对外接口：读取当前上传状态快照，供上层查询进度/结果。 */
int log_upload_get_state_snapshot(log_upload_state_t *state)
{
    if (!state) {
        return -EINVAL;
    }

    taskENTER_CRITICAL();
    *state = s_log_upload_state;
    taskEXIT_CRITICAL();

    return 0;
}

/* 对外接口：判断当前是否已经有上传任务在运行。 */
int log_upload_is_running(void)
{
    return s_log_upload_running ? 1 : 0;
}
