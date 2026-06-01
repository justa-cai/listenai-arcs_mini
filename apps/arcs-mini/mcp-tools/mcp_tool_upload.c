#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TAG "mcp.tool.upload_log"

#include "cJSON.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "log_upload.h"
#include "mcp.h"

#define UPLOAD_LOG_TOOL_NAME "ls.built_in.upload_log"

struct upload_log_async_ctx {
    char *id;
};

static void upload_log_async_ctx_destroy(struct upload_log_async_ctx *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->id) {
        lisa_mem_free(ctx->id);
    }
    lisa_mem_free(ctx);
}

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

static cJSON *upload_log_tool_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *upload_url_json = mcp_tool_call_args_get(args, "upload_url");
    struct upload_log_async_ctx *ctx = NULL;
    int ret;

    if (!id || id[0] == '\0') {
        LOGE("upload log failed: invalid mcp id");
        return upload_log_result_text(name, "MCP 调用 id 无效。", true);
    }

    if (!upload_url_json || !cJSON_IsString(upload_url_json) || !upload_url_json->valuestring ||
        upload_url_json->valuestring[0] == '\0') {
        LOGE("upload log failed: invalid upload_url");
        return upload_log_result_text(name, "upload_url 参数缺失或格式错误。", true);
    }

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    }

    ctx->id = lisa_mem_calloc(1, strlen(id) + 1);
    if (!ctx->id) {
        upload_log_async_ctx_destroy(ctx);
        return upload_log_result_text(name, "内存不足，无法上传日志。", true);
    }
    strcpy(ctx->id, id);

    ret = log_upload_trigger_with_url(upload_url_json->valuestring, upload_log_complete, ctx);
    if (ret != 0) {
        LOGE("upload log trigger failed (%d)", ret);
        upload_log_async_ctx_destroy(ctx);
        return upload_log_error_result(name, ret);
    }

    LOGI("upload log request accepted, id=%s", id);
    return NULL;
}

MCP_TOOL_DEFINE(ls.built_in.upload_log, upload_log_tool_list, upload_log_tool_call);
