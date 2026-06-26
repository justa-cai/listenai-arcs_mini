#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TAG "mcp.tool.upload_log"

#include "cJSON.h"
#include "async_task.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "log_upload.h"
#include "mcp.h"

#define UPLOAD_LOG_TOOL_NAME "ls.built_in.upload_log"
#define UPLOAD_LOG_SCHEDULE_TASK_STACK_SIZE 4096U
#define UPLOAD_LOG_SCHEDULE_TASK_PRIORITY 5U

struct upload_log_async_ctx {
    char *id;
    char *upload_url;
};

/* 释放一次 MCP 上传请求绑定的上下文。 */
static void upload_log_async_ctx_destroy(struct upload_log_async_ctx *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->id) {
        lisa_mem_free(ctx->id);
    }
    if (ctx->upload_url) {
        lisa_mem_free(ctx->upload_url);
    }
    lisa_mem_free(ctx);
}

/* 复制 MCP 参数字符串，避免异步线程访问已释放的 JSON 内存。 */
static char *upload_log_strdup(const char *src)
{
    char *dst;

    if (!src) {
        return NULL;
    }

    dst = lisa_mem_calloc(1, strlen(src) + 1);
    if (!dst) {
        return NULL;
    }

    strcpy(dst, src);
    return dst;
}

/* 组装 MCP 文本结果，供工具列表/工具调用统一返回。 */
static cJSON *upload_log_result_text(const char *name, const char *text, bool is_error)
{
    cJSON *result = mcp_tool_call_result_create(name);
    cJSON *content_array = NULL;
    cJSON *content_item = NULL;

    if (!result) {
        return NULL;
    }

    content_array = cJSON_CreateArray();
    content_item = cJSON_CreateObject();
    if (!content_array || !content_item) {
        cJSON_Delete(content_array);
        cJSON_Delete(content_item);
        cJSON_Delete(result);
        return NULL;
    }

    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", is_error);

    return result;
}

/* 把底层上传错误码转换成对用户更友好的中文提示。 */
static cJSON *upload_log_error_result(const char *name, int ret)
{
    char text[96];

    switch (ret) {
    case -EINVAL:
        return upload_log_result_text(name, "upload_url 参数缺失或格式错误。", true);
    case -EBUSY:
        return upload_log_result_text(name, "已有日志上传任务正在进行，请稍后重试。", true);
    case -ENOMEM:
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    case -ENOSPC:
        return upload_log_result_text(name, "upload_url 过长，无法发起上传。", true);
    case -ENODATA:
        return upload_log_result_text(name, "当前没有可上传的日志。", true);
    default:
        if (ret > 0) {
            snprintf(text, sizeof(text), "日志上传失败，HTTP 状态码: %d", ret);
        } else {
            snprintf(text, sizeof(text), "日志上传失败，错误码: %d", ret);
        }
        return upload_log_result_text(name, text, true);
    }
}

/* 对外暴露 upload_log 工具的描述与参数定义。 */
static cJSON *upload_log_tool_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "上传设备日志到云端，用于问题排查和现场日志回收");

    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(
        tool,
        "upload_url",
        "用于上传设备日志的一次性 URL",
        "string",
        true);

    return tool;
}

/* 上传任务结束后，把结果回包给当前这次 MCP 调用。 */
static void upload_log_complete(const log_upload_state_t *state, void *arg)
{
    struct upload_log_async_ctx *ctx = (struct upload_log_async_ctx *)arg;
    cJSON *result = NULL;

    if (!ctx || !ctx->id || ctx->id[0] == '\0') {
        upload_log_async_ctx_destroy(ctx);
        return;
    }
    if (state && state->state == LOG_UPLOAD_STATE_SUCCESSED) {
        result = upload_log_result_text(UPLOAD_LOG_TOOL_NAME, "日志上传成功。", false);
    } else if (state) {
        result = upload_log_error_result(UPLOAD_LOG_TOOL_NAME, state->result);
    } else {
        result = upload_log_result_text(UPLOAD_LOG_TOOL_NAME, "日志上传失败。", true);
    }

    if (result) {
        LOGI("upload log async response, id=%s, state=%d, result=%d",
             ctx->id,
             state ? state->state : -1,
             state ? state->result : -1);
        mcp_tool_call_result_response(ctx->id, result);
        cJSON_Delete(result);
    } else {
        LOGE("upload log async response alloc failed");
    }

    upload_log_async_ctx_destroy(ctx);
}

/* 在独立任务里触发日志上传，避免 MCP 所在的 voice.ebus 被上传调度阻塞。 */
static void upload_log_schedule_task(void *user_data, bool *should_stop)
{
    struct upload_log_async_ctx *ctx = (struct upload_log_async_ctx *)user_data;
    cJSON *result = NULL;
    int ret;

    (void)should_stop;

    if (!ctx || !ctx->id || !ctx->upload_url) {
        upload_log_async_ctx_destroy(ctx);
        return;
    }

    ret = log_upload_trigger(ctx->upload_url, upload_log_complete, ctx);
    if (ret == 0) {
        return;
    }

    LOGE("upload log trigger failed (%d)", ret);
    result = upload_log_error_result(UPLOAD_LOG_TOOL_NAME, ret);
    if (result) {
        mcp_tool_call_result_response(ctx->id, result);
        cJSON_Delete(result);
    } else {
        LOGE("upload log trigger response alloc failed");
    }

    upload_log_async_ctx_destroy(ctx);
}

/* 处理外部发起的 MCP tools/call，并异步启动一次日志上传。 */
static cJSON *upload_log_tool_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *upload_url_json;
    const char *upload_url;
    struct upload_log_async_ctx *ctx;
    async_task_t *task;

    if (!id || id[0] == '\0') {
        LOGE("upload log failed: invalid mcp id");
        return upload_log_result_text(name, "MCP 调用 id 无效。", true);
    }

    upload_url_json = mcp_tool_call_args_get(args, "upload_url");
    if (!upload_url_json || !cJSON_IsString(upload_url_json) || !upload_url_json->valuestring ||
        upload_url_json->valuestring[0] == '\0') {
        LOGE("upload log failed: invalid upload_url");
        return upload_log_result_text(name, "upload_url 参数缺失或格式错误。", true);
    }

    upload_url = upload_url_json->valuestring;
    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    }

    ctx->id = upload_log_strdup(id);
    ctx->upload_url = upload_log_strdup(upload_url);
    if (!ctx->id || !ctx->upload_url) {
        upload_log_async_ctx_destroy(ctx);
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    }

    task = async_task_create("upload_sched",
                             UPLOAD_LOG_SCHEDULE_TASK_STACK_SIZE,
                             UPLOAD_LOG_SCHEDULE_TASK_PRIORITY,
                             upload_log_schedule_task,
                             NULL,
                             ctx);
    if (!task) {
        upload_log_async_ctx_destroy(ctx);
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    }

    if (async_task_start(task) != 0) {
        async_task_destroy(task);
        upload_log_async_ctx_destroy(ctx);
        return upload_log_result_text(name, "日志上传任务启动失败。", true);
    }

    LOGI("upload log request accepted, id=%s", id);
    return NULL;
}

MCP_TOOL_DEFINE(ls.built_in.upload_log, upload_log_tool_list, upload_log_tool_call);
