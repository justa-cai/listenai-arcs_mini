/*
 * MCP 工具 rom_load: 把 PC 端 ROM 库里的某个 ROM 加载到设备上运行。
 *
 * 复用已经跑通的"WS 推送 ROM"通路 —— 只是字节来源从 WebSocket 二进制帧
 * 换成 HTTP 流式下载:
 *   WS 侧 (§4.2.1)        : rom_begin -> [二进制帧] -> rom_data -> rom_end -> rom_ack
 *   本工具                : rom_begin -> [HTTP 分块] -> rom_data -> rom_end
 * 两者共用的 gamepad_rom_begin/data/end 负责 PSRAM 暂存、iNES/crc 校验与
 * staging 换代; present 的 LVGL tick 发现代数变化后热重启模拟器, 因此
 * 加载完游戏会自动切到新 ROM, 无需额外通知。
 *
 * 调用跑在 voice.ebus 单线程上, 不能阻塞做下载, 所以走异步:
 * call 返回 NULL, 独立任务完成后用 mcp_tool_call_result_response 回包。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "async_task.h"
#include "cJSON.h"
#include "gamepad_rom.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "mcp.h"
#include "rom_api.h"

#define TAG "mcp.tool.rom_load"

#define ROM_LOAD_TOOL_NAME     "rom_load"
#define ROM_LOAD_TASK_STACK    8192u
#define ROM_LOAD_TASK_PRIORITY 5u

struct rom_load_ctx {
    char *id;
    char *rel_path;
};

/* ------------------------------------------------------------------ */
/* 上下文 / 结果组装                                                     */
/* ------------------------------------------------------------------ */

static void rom_load_ctx_destroy(struct rom_load_ctx *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->id) {
        lisa_mem_free(ctx->id);
    }
    if (ctx->rel_path) {
        lisa_mem_free(ctx->rel_path);
    }
    lisa_mem_free(ctx);
}

/* 复制 MCP 参数字符串, 避免异步任务访问已释放的 JSON 内存。 */
static char *rom_load_strdup(const char *src)
{
    size_t len;
    char *dst;

    if (!src) {
        return NULL;
    }
    len = strlen(src) + 1;
    dst = lisa_mem_calloc(1, (uint32_t)len);
    if (!dst) {
        return NULL;
    }
    memcpy(dst, src, len);
    return dst;
}

static cJSON *rom_load_result_text(const char *text, bool is_error, cJSON *detail)
{
    cJSON *result = mcp_tool_call_result_create(ROM_LOAD_TOOL_NAME);
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

    if (detail) {
        cJSON_AddItemToObject(result, "result", detail);
    }

    return result;
}

static cJSON *rom_load_error_result(int ret, const char *stage)
{
    char text[256];

    switch (ret) {
    case ROM_API_ERR_NO_BASE:
        return rom_load_result_text(
            "没有可用的 ROM 库地址：请先在 PC 端启动 pad_gui 并点一次\"扫描\""
            "（设备会从 UDP 发现探测里记住它的地址），或在设备串口执行 "
            "kv set string user.rom_api_url http://<PC-IP>:38202。",
            true, NULL);
    case ROM_API_ERR_NOFILE:
        return rom_load_result_text(
            "ROM 库里没有这个文件（可能已改名/移动）。请重新用 rom_search 查询后再加载。",
            true, NULL);
    case ROM_API_ERR_NET:
        return rom_load_result_text(
            "连接 ROM 库服务失败：请确认 PC 端 pad_gui 正在运行，且与设备在同一局域网。",
            true, NULL);
    case ROM_API_ERR_ABORTED:
        return rom_load_result_text(
            "下载中断：收到的数据与 ROM 声明大小不符（文件在下载过程中被改动？）。",
            true, NULL);
    default:
        snprintf(text, sizeof(text), "加载 ROM 失败（%s阶段，错误码 %d）。",
                 stage ? stage : "未知", ret);
        return rom_load_result_text(text, true, NULL);
    }
}

/* ------------------------------------------------------------------ */
/* 分块回调: 把 HTTP 收到的字节喂给 gamepad_rom 的暂存缓冲                */
/* ------------------------------------------------------------------ */

static int rom_load_chunk(const uint8_t *data, uint32_t len, void *user)
{
    (void)user;
    /* 返回非 0 → rom_api_download 停止回调 (溢出时 gamepad_rom 内部已 cancel) */
    return (gamepad_rom_data(data, len) == 0) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* 异步任务                                                             */
/* ------------------------------------------------------------------ */

static void rom_load_worker(void *user_data, bool *should_stop)
{
    struct rom_load_ctx *ctx = (struct rom_load_ctx *)user_data;
    rom_api_stat_t st;
    cJSON *result = NULL;
    cJSON *detail = NULL;
    char text[256];
    uint32_t got = 0;
    int ret;

    (void)should_stop;

    if (!ctx || !ctx->id || !ctx->rel_path) {
        rom_load_ctx_destroy(ctx);
        return;
    }

    /* ① 取详情: 必须先把准确大小问出来, 才能分配暂存缓冲 */
    ret = rom_api_stat(ctx->rel_path, &st);
    if (ret != ROM_API_OK) {
        LOGE("rom load stat failed (%d): %s", ret, ctx->rel_path);
        result = rom_load_error_result(ret, "查询详情");
        goto respond;
    }
    if (st.size_bytes < 16U || st.size_bytes > GAMEPAD_ROM_MAX_BYTES) {
        LOGE("rom load bad size %u: %s", (unsigned)st.size_bytes, ctx->rel_path);
        snprintf(text, sizeof(text),
                 "该 ROM 大小 %u 字节，超出可加载范围（16B ~ %uKB）。",
                 (unsigned)st.size_bytes, (unsigned)(GAMEPAD_ROM_MAX_BYTES / 1024U));
        result = rom_load_result_text(text, true, NULL);
        goto respond;
    }
    LOGI("rom load: %s (%u bytes)", ctx->rel_path, (unsigned)st.size_bytes);

    /* ② 开始暂存。可能被 pad_gui 的 WS 推送占用 → 直接报错, 不抢 */
    if (gamepad_rom_begin(st.size_bytes, 0U) != 0) {
        result = rom_load_result_text(
            "设备正在接收另一路 ROM 推送（pad_gui 推送中），请稍后重试。", true, NULL);
        goto respond;
    }

    /* ③ 流式下载, 边收边写入暂存缓冲 (不额外保存一份) */
    ret = rom_api_download(ctx->rel_path, rom_load_chunk, NULL, &got);
    if (ret != ROM_API_OK) {
        gamepad_rom_cancel();
        LOGE("rom load download failed (%d), got %u/%u", ret, (unsigned)got,
             (unsigned)st.size_bytes);
        result = rom_load_error_result(ret, "下载");
        goto respond;
    }

    /* ④ 校验 (大小 + iNES 魔数) 并换入 staging; 成功后代数 +1,
     *    游戏屏的下一次 tick 会检测到并热重启模拟器 */
    const char *err = NULL;
    if (gamepad_rom_end(&err) != 0) {
        snprintf(text, sizeof(text), "ROM 校验失败：%s。", (err && err[0]) ? err : "数据不完整");
        LOGE("rom load end failed: %s", (err && err[0]) ? err : "-");
        result = rom_load_result_text(text, true, NULL);
        goto respond;
    }

    snprintf(text, sizeof(text),
             "已加载《%s》（%uKB，%s），游戏已切换到该 ROM 并重新开始。",
             (st.name[0] != '\0') ? st.name : ctx->rel_path,
             (unsigned)((st.size_bytes + 1023U) / 1024U), ctx->rel_path);

    detail = cJSON_CreateObject();
    if (detail) {
        cJSON_AddBoolToObject(detail, "ok", true);
        cJSON_AddStringToObject(detail, "name", st.name);
        cJSON_AddStringToObject(detail, "rel_path", ctx->rel_path);
        cJSON_AddNumberToObject(detail, "size_bytes", (double)st.size_bytes);
        if (st.mapper >= 0) {
            cJSON_AddNumberToObject(detail, "mapper", (double)st.mapper);
        } else {
            cJSON_AddNullToObject(detail, "mapper");
        }
    }

    LOGI("rom load ok: %s (%u bytes)", ctx->rel_path, (unsigned)st.size_bytes);
    result = rom_load_result_text(text, false, detail);

respond:
    if (!result) {
        /* 结果组装失败也必须回包, 否则这次 MCP 调用会一直挂着等响应 */
        LOGE("rom load result alloc failed");
        result = rom_load_result_text("加载已完成但结果组装失败（内存不足）。", true, NULL);
    }
    if (result) {
        mcp_tool_call_result_response(ctx->id, result);
        cJSON_Delete(result);
    }
    rom_load_ctx_destroy(ctx);
}

/* ------------------------------------------------------------------ */
/* 工具定义                                                             */
/* ------------------------------------------------------------------ */

static cJSON *rom_load_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "把 PC 端 ROM 库里的某个游戏加载到设备上运行（设备会自动重启游戏并切换到它）。"
        "先用 rom_search 查到候选，再把选中的 rel_path 传给本工具。"
        "只支持 iNES/NES2.0 镜像，大小上限 1MB。");

    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(
        tool,
        "rel_path",
        "要加载的 ROM 标识，取自 rom_search 返回的 result.items[].rel_path（即相对路径）",
        "string",
        true);

    return tool;
}

static cJSON *rom_load_call(const char *id, const char *name, cJSON *args)
{
    struct rom_load_ctx *ctx;
    async_task_t *task;

    (void)name;

    if (!id || id[0] == '\0') {
        LOGE("rom load failed: invalid mcp id");
        return rom_load_result_text("MCP 调用 id 无效。", true, NULL);
    }

    const cJSON *rel_json = mcp_tool_call_args_get(args, "rel_path");
    if (!rel_json || !cJSON_IsString(rel_json) || !rel_json->valuestring ||
        rel_json->valuestring[0] == '\0') {
        LOGE("rom load failed: invalid rel_path");
        return rom_load_result_text(
            "缺少 rel_path 参数；请先用 rom_search 查询，再把它返回的 rel_path 传进来。",
            true, NULL);
    }

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return rom_load_result_text("内存不足，无法加载 ROM。", true, NULL);
    }

    ctx->id = rom_load_strdup(id);
    ctx->rel_path = rom_load_strdup(rel_json->valuestring);
    if (!ctx->id || !ctx->rel_path) {
        rom_load_ctx_destroy(ctx);
        return rom_load_result_text("内存不足，无法加载 ROM。", true, NULL);
    }

    task = async_task_create("romload_sched", ROM_LOAD_TASK_STACK, ROM_LOAD_TASK_PRIORITY,
                             rom_load_worker, NULL, ctx);
    if (!task) {
        rom_load_ctx_destroy(ctx);
        return rom_load_result_text("内存不足，无法加载 ROM。", true, NULL);
    }

    if (async_task_start(task) != 0) {
        async_task_destroy(task);
        rom_load_ctx_destroy(ctx);
        return rom_load_result_text("ROM 加载任务启动失败。", true, NULL);
    }

    LOGI("rom load accepted: id=%s rel_path=%s", id, ctx->rel_path);
    return NULL;    /* 异步: 结果稍后经 mcp_tool_call_result_response 回包 */
}

MCP_TOOL_DEFINE(rom_load, rom_load_list, rom_load_call);
