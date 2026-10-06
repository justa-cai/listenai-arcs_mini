/*
 * MCP 工具 rom_search: 按关键词查询 PC 端 ROM 库 (pad_gui.py 的只读 HTTP API)。
 *
 * 用户说"加载魂斗罗"时, 云端大模型从自然语言抽出关键词 (如"魂斗罗"),
 * 调本工具拿到候选清单, 再据此决定后续动作 (本次仅查询, 不做加载)。
 *
 * 调用跑在 voice.ebus 单线程上, 不能阻塞做 HTTP, 所以走异步:
 * call 返回 NULL, 独立任务完成后用 mcp_tool_call_result_response 回包。
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "async_task.h"
#include "cJSON.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "mcp.h"
#include "rom_api.h"

#define TAG "mcp.tool.rom_search"

#define ROM_SEARCH_TOOL_NAME     "rom_search"
#define ROM_SEARCH_DEFAULT_LIMIT 10u
#define ROM_SEARCH_MAX_LIMIT     50u
#define ROM_SEARCH_TASK_STACK    6144u
#define ROM_SEARCH_TASK_PRIORITY 5u

/* 给大模型看的候选文本上限 (堆上分配, 不占任务栈); 结构化 items 不受此限。 */
#define ROM_SEARCH_TEXT_MAX      2048
/* 文本尾部预留: 用于"另有 N 个未列出"的截断说明 */
#define ROM_SEARCH_TEXT_RESERVE  160

struct rom_search_ctx {
    char *id;
    char *query;
    uint32_t limit;
};

/* ------------------------------------------------------------------ */
/* 上下文管理                                                           */
/* ------------------------------------------------------------------ */

static void rom_search_ctx_destroy(struct rom_search_ctx *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->id) {
        lisa_mem_free(ctx->id);
    }
    if (ctx->query) {
        lisa_mem_free(ctx->query);
    }
    lisa_mem_free(ctx);
}

/* 复制 MCP 参数字符串, 避免异步任务访问已释放的 JSON 内存。 */
static char *rom_search_strdup(const char *src)
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

/* ------------------------------------------------------------------ */
/* 结果组装                                                             */
/* ------------------------------------------------------------------ */

static cJSON *rom_search_result_text(const char *text, bool is_error, cJSON *detail)
{
    cJSON *result = mcp_tool_call_result_create(ROM_SEARCH_TOOL_NAME);
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

/* 把 romlib 错误码翻译成对用户/大模型可读的中文提示。 */
static cJSON *rom_search_error_result(int ret)
{
    switch (ret) {
    case ROM_API_ERR_NO_BASE:
        return rom_search_result_text(
            "没有可用的 ROM 库地址：请先在 PC 端启动 pad_gui 并点一次\"扫描\""
            "（设备会从 UDP 发现探测里记住它的地址），或在设备串口执行 "
            "kv set string user.rom_api_url http://<PC-IP>:38202。",
            true, NULL);
    case ROM_API_ERR_NET:
        return rom_search_result_text(
            "连接 ROM 库服务失败：请确认 PC 端 pad_gui 正在运行，且与设备在同一局域网。",
            true, NULL);
    case ROM_API_ERR_PARAM:
        return rom_search_result_text("查询参数无效（关键词为空或过长）。", true, NULL);
    case ROM_API_ERR_HTTP:
        return rom_search_result_text("设备 HTTP 客户端初始化失败，无法查询 ROM 库。", true, NULL);
    case ROM_API_ERR_PARSE:
        return rom_search_result_text("ROM 库返回内容无法解析，请确认 pad_gui 版本支持 /api/roms。", true, NULL);
    case ROM_API_ERR_NOMEM:
        return rom_search_result_text("内存不足，无法完成查询。", true, NULL);
    default:
        return rom_search_result_text("查询 ROM 库失败。", true, NULL);
    }
}

/* 成功结果: content 列出候选 (人/模型可读), result 挂结构化清单 (便于精确取用)。 */
static cJSON *rom_search_build_result(const struct rom_search_ctx *ctx, const rom_api_result_t *res)
{
    char *text = lisa_mem_calloc(1, ROM_SEARCH_TEXT_MAX);
    cJSON *detail = NULL;
    cJSON *items = NULL;
    size_t used = 0;
    uint32_t shown = 0;

    if (!text) {
        return rom_search_error_result(ROM_API_ERR_NOMEM);
    }

    int n = snprintf(text, ROM_SEARCH_TEXT_MAX,
                     "关键词 \"%s\"：ROM 库共 %u 个，命中 %u 个，本次返回 %u 个。\n",
                     ctx->query, (unsigned)res->total, (unsigned)res->matched, (unsigned)res->count);
    if (n > 0) {
        used = ((size_t)n < ROM_SEARCH_TEXT_MAX) ? (size_t)n : ROM_SEARCH_TEXT_MAX - 1;
    }

    if (res->count == 0) {
        snprintf(text + used, ROM_SEARCH_TEXT_MAX - used,
                 "没有匹配的 ROM，可换用更短或更常见的关键词重试。\n");
    }

    detail = cJSON_CreateObject();
    items = cJSON_CreateArray();
    if (!items || !detail) {
        cJSON_Delete(items);
        cJSON_Delete(detail);
        lisa_mem_free(text);
        return rom_search_error_result(ROM_API_ERR_NOMEM);
    }

    for (uint32_t i = 0; i < res->count; i++) {
        const rom_api_item_t *it = &res->items[i];
        char mapper[16];
        const char *mapper_str;
        size_t avail;

        if (it->mapper >= 0) {
            snprintf(mapper, sizeof(mapper), "%d", it->mapper);
            mapper_str = mapper;
        } else {
            mapper_str = "-";
        }

        /* 结构化条目: 名称 / 路径(即 API 的 id) / 大小 / mapper */
        cJSON *entry = cJSON_CreateObject();
        if (entry) {
            cJSON_AddStringToObject(entry, "name", it->name);
            cJSON_AddStringToObject(entry, "rel_path", it->rel_path);
            cJSON_AddNumberToObject(entry, "size_kb", (double)it->size_kb);
            if (it->mapper >= 0) {
                cJSON_AddNumberToObject(entry, "mapper", (double)it->mapper);
            } else {
                cJSON_AddNullToObject(entry, "mapper");
            }
            cJSON_AddItemToArray(items, entry);
        }

        /* 文本候选: 受 text 缓冲上限约束。留出 ROM_SEARCH_TEXT_RESERVE 字节
         * 给尾部提示, 保证截断说明一定能写进去 (完整清单始终在 result.items) */
        if (used + 1 >= ROM_SEARCH_TEXT_MAX - ROM_SEARCH_TEXT_RESERVE) {
            break;
        }
        avail = (ROM_SEARCH_TEXT_MAX - ROM_SEARCH_TEXT_RESERVE) - used;
        int ln = snprintf(text + used, avail, "%u. %s | %uKB | mapper %s | %s\n",
                          (unsigned)(i + 1), it->name, (unsigned)it->size_kb, mapper_str,
                          it->rel_path);
        used = strlen(text);
        shown++;
        if (ln < 0 || (size_t)ln >= avail) {
            break;  /* 单行都放不下 (超长路径), 停止追加 */
        }
    }

    if (shown < res->count) {
        snprintf(text + used, ROM_SEARCH_TEXT_MAX - used,
                 "（另有 %u 个候选未在文本中列出，完整清单见 result.items）\n",
                 (unsigned)(res->count - shown));
    }

    cJSON_AddNumberToObject(detail, "total", (double)res->total);
    cJSON_AddNumberToObject(detail, "matched", (double)res->matched);
    cJSON_AddNumberToObject(detail, "returned", (double)res->count);
    cJSON_AddItemToObject(detail, "items", items);

    cJSON *result = rom_search_result_text(text, false, detail);
    lisa_mem_free(text);
    if (!result) {
        cJSON_Delete(detail);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* 异步任务                                                             */
/* ------------------------------------------------------------------ */

static void rom_search_worker(void *user_data, bool *should_stop)
{
    struct rom_search_ctx *ctx = (struct rom_search_ctx *)user_data;
    rom_api_result_t res;
    cJSON *result = NULL;

    (void)should_stop;

    if (!ctx || !ctx->id || !ctx->query) {
        rom_search_ctx_destroy(ctx);
        return;
    }

    int ret = rom_api_search(ctx->query, ctx->limit, &res);
    if (ret != ROM_API_OK) {
        LOGE("rom search failed (%d), query=%s", ret, ctx->query);
        result = rom_search_error_result(ret);
    } else {
        LOGI("rom search ok: query=%s matched=%u returned=%u", ctx->query, (unsigned)res.matched,
             (unsigned)res.count);
        result = rom_search_build_result(ctx, &res);
        rom_api_result_free(&res);
    }

    if (!result) {
        /* 结果组装失败也必须回包, 否则这次 MCP 调用会一直挂着等响应 */
        LOGE("rom search result alloc failed");
        result = rom_search_result_text("查询已完成但结果组装失败（内存不足）。", true, NULL);
    }

    if (result) {
        mcp_tool_call_result_response(ctx->id, result);
        cJSON_Delete(result);
    }

    rom_search_ctx_destroy(ctx);
}

/* ------------------------------------------------------------------ */
/* 工具定义                                                             */
/* ------------------------------------------------------------------ */

static cJSON *rom_search_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "按名称关键词查询 PC 端 ROM 库（pad_gui 提供的只读 ROM 列表），返回候选游戏清单供选择。"
        "用户说\"加载/找/玩某个游戏\"时，先用本工具检索有哪些可选项；不含设备 flash 内置 ROM。");

    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(
        tool,
        "query",
        "游戏名关键词，如 \"魂斗罗\"、\"Contra\"、\"超级马里奥\"；支持中英文子串匹配，大小写不敏感",
        "string",
        true);

    cJSON *limit_property = cJSON_CreateObject();
    if (!limit_property) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddStringToObject(limit_property, "type", "integer");
    cJSON_AddStringToObject(limit_property, "description", "返回候选条数上限，1..50，默认 10");
    cJSON_AddNumberToObject(limit_property, "minimum", 1);
    cJSON_AddNumberToObject(limit_property, "maximum", (double)ROM_SEARCH_MAX_LIMIT);
    mcp_tool_info_add_json_property(tool, "limit", limit_property, false);

    return tool;
}

static cJSON *rom_search_call(const char *id, const char *name, cJSON *args)
{
    struct rom_search_ctx *ctx;
    async_task_t *task;
    uint32_t limit = ROM_SEARCH_DEFAULT_LIMIT;

    (void)name;

    if (!id || id[0] == '\0') {
        LOGE("rom search failed: invalid mcp id");
        return rom_search_result_text("MCP 调用 id 无效。", true, NULL);
    }

    const cJSON *query_json = mcp_tool_call_args_get(args, "query");
    if (!query_json || !cJSON_IsString(query_json) || !query_json->valuestring ||
        query_json->valuestring[0] == '\0') {
        LOGE("rom search failed: invalid query");
        return rom_search_result_text("缺少 query 参数（游戏名关键词）。", true, NULL);
    }

    const cJSON *limit_json = mcp_tool_call_args_get(args, "limit");
    if (limit_json && cJSON_IsNumber(limit_json)) {
        int v = limit_json->valueint;
        if (v < 1) {
            v = 1;
        }
        if (v > (int)ROM_SEARCH_MAX_LIMIT) {
            v = (int)ROM_SEARCH_MAX_LIMIT;
        }
        limit = (uint32_t)v;
    }

    ctx = lisa_mem_calloc(1, sizeof(*ctx));
    if (!ctx) {
        return rom_search_error_result(ROM_API_ERR_NOMEM);
    }

    ctx->id = rom_search_strdup(id);
    ctx->query = rom_search_strdup(query_json->valuestring);
    ctx->limit = limit;
    if (!ctx->id || !ctx->query) {
        rom_search_ctx_destroy(ctx);
        return rom_search_error_result(ROM_API_ERR_NOMEM);
    }

    task = async_task_create("rom_sched", ROM_SEARCH_TASK_STACK, ROM_SEARCH_TASK_PRIORITY,
                             rom_search_worker, NULL, ctx);
    if (!task) {
        rom_search_ctx_destroy(ctx);
        return rom_search_error_result(ROM_API_ERR_NOMEM);
    }

    if (async_task_start(task) != 0) {
        async_task_destroy(task);
        rom_search_ctx_destroy(ctx);
        return rom_search_result_text("ROM 查询任务启动失败。", true, NULL);
    }

    LOGI("rom search accepted: id=%s query=%s limit=%u", id, ctx->query, (unsigned)limit);
    return NULL;    /* 异步: 结果稍后经 mcp_tool_call_result_response 回包 */
}

MCP_TOOL_DEFINE(rom_search, rom_search_list, rom_search_call);
