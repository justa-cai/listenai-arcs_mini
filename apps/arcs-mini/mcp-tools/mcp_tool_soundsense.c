/*
 * mcp_tool_soundsense.c - MCP tools for the SoundSense detection system
 *
 * Three tools (bare names, no ls.* prefix per SKILL.md):
 *   soundsense_status  : device monitoring state + per-class totals + live probs
 *   soundsense_events  : recent detection events from the device ring buffer
 *   soundsense_server  : forwarded query to the LAN SoundSense server (R2/R3)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "cJSON.h"
#include "lisa_http.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "mcp.h"

#include "soundsense/ss_core.h"

#define TAG "ss"

/* ------------------------------------------------------------------ */
/* soundsense_status                                                   */
/* ------------------------------------------------------------------ */

static const char *const k_state_names[] = {
    [SS_STATE_OFF] = "off",
    [SS_STATE_RETRY_WAIT] = "retry_wait",
    [SS_STATE_CONNECTING] = "connecting",
    [SS_STATE_STREAMING] = "streaming",
};

static cJSON *soundsense_status_list(const char *name)
{
    return mcp_tool_list_info_create_default(name,
        "查询设备端异常声音检测（哭声/鼾声）的当前状态。返回：监测开关、连接状态、"
        "服务器地址、运行时长、音频帧统计、每类声音的累计检测次数、最近事件时间、"
        "当前实时检测概率。用户问'有没有检测到哭声'、'鼾声检测状态'、'声音监测怎么样'时调用。");
}

static void ss_append_class_summary(cJSON *obj, const char *key, int idx,
                                     const ss_status_t *st)
{
    cJSON *cls = cJSON_CreateObject();
    cJSON_AddNumberToObject(cls, "event_count", st->class_event_count[idx]);
    cJSON_AddNumberToObject(cls, "probability", st->last_probs_x1000[idx] / 1000.0);

    if (st->class_last_time[idx][0] != '\0') {
        cJSON_AddStringToObject(cls, "last_event", st->class_last_time[idx]);
        cJSON_AddBoolToObject(cls, "ongoing", st->class_last_is_start[idx]);
    } else {
        cJSON_AddNullToObject(cls, "last_event");
        cJSON_AddFalseToObject(cls, "ongoing");
    }
    cJSON_AddItemToObject(obj, key, cls);
}

static cJSON *soundsense_status_call(const char *id, const char *name, cJSON *args)
{
    (void)id;
    (void)args;

    ss_status_t st;
    ss_core_get_status(&st);

    char text[512];
    snprintf(text, sizeof(text),
             "声音监测状态：\n"
             "  监测：%s\n"
             "  连接：%s\n"
             "  服务器：%s\n"
             "  运行时长：%u秒\n"
             "  音频帧：发送%u，丢弃%u\n"
             "  推理结果：%u条，重连%u次\n"
             "  鼾声：检测到%u次，当前概率%.1f%%%s\n"
             "  哭声：检测到%u次，当前概率%.1f%%%s",
             st.enabled ? "开启" : "关闭",
             k_state_names[st.state],
             st.server_url,
             st.uptime_s,
             st.send_frames, st.send_drops,
             st.result_count, st.reconnect_count,
             st.class_event_count[0],
             st.last_probs_x1000[0] / 10.0,
             st.class_last_time[0][0] != '\0' ? "，最近有检测" : "",
             st.class_event_count[1],
             st.last_probs_x1000[1] / 10.0,
             st.class_last_time[1][0] != '\0' ? "，最近有检测" : "");

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) return NULL;
    cJSON_AddStringToObject(result, "content", text);
    cJSON_AddBoolToObject(result, "isError", false);
    return result;
}

/* ------------------------------------------------------------------ */
/* soundsense_events                                                   */
/* ------------------------------------------------------------------ */

static cJSON *soundsense_events_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "查询设备端最近的声音检测事件列表（哭声/鼾声的开始和结束事件）。"
        "用户问'最近检测到什么声音'、'哭声是什么时候'、'鼾声发生了几次'时调用。");
    mcp_tool_info_add_property(tool, "count", "返回事件条数（默认10，最大64）", "integer", false);
    mcp_tool_info_add_property(tool, "class", "过滤声音类型：snoring 或 baby_cry（可选）",
                               "string", false);
    return tool;
}

static cJSON *soundsense_events_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    int count = 10;
    const char *class_filter = NULL;

    const cJSON *count_json = mcp_tool_call_args_get(args, "count");
    if (cJSON_IsNumber(count_json) && count_json->valueint > 0) {
        count = count_json->valueint;
    }
    if (count > SS_EVENT_RING) count = SS_EVENT_RING;

    const cJSON *class_json = mcp_tool_call_args_get(args, "class");
    if (cJSON_IsString(class_json) && class_json->valuestring[0] != '\0') {
        if (strcmp(class_json->valuestring, "snoring") != 0 &&
            strcmp(class_json->valuestring, "baby_cry") != 0) {
            cJSON *result = mcp_tool_call_result_create(name);
            cJSON_AddStringToObject(result, "content", "class 参数必须是 snoring 或 baby_cry");
            cJSON_AddBoolToObject(result, "isError", true);
            return result;
        }
        class_filter = class_json->valuestring;
    }

    ss_event_t *events = lisa_mem_alloc(sizeof(ss_event_t) * count);
    if (!events) {
        cJSON *result = mcp_tool_call_result_create(name);
        cJSON_AddStringToObject(result, "content", "内存不足");
        cJSON_AddBoolToObject(result, "isError", true);
        return result;
    }
    uint32_t n = ss_core_get_events(events, count, class_filter);

    char text[1024] = "最近检测事件：\n";
    if (n == 0) {
        strncat(text, "  （无事件）", sizeof(text) - strlen(text) - 1);
    } else {
        for (uint32_t i = 0; i < n && strlen(text) < sizeof(text) - 80; i++) {
            char line[96];
            char time_str[20];
            time_t sec = events[i].ts_ms / 1000 + 8 * 3600;
            struct tm tm_s;
            gmtime_r(&sec, &tm_s);
            snprintf(time_str, sizeof(time_str), "%02d:%02d", tm_s.tm_hour, tm_s.tm_min);

            snprintf(line, sizeof(line), "  [%s] %s %s 持续%u.%01us 峰值%u%%\n",
                     time_str,
                     events[i].class_name,
                     events[i].is_start ? "开始" : "结束",
                     events[i].duration_ms / 1000,
                     (events[i].duration_ms % 1000) / 100,
                     events[i].peak_prob_x100);
            strncat(text, line, sizeof(text) - strlen(text) - 1);
        }
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) { lisa_mem_free(events); return NULL; }
    cJSON_AddStringToObject(result, "content", text);
    cJSON_AddBoolToObject(result, "isError", false);
    lisa_mem_free(events);
    return result;
}

/* ------------------------------------------------------------------ */
/* soundsense_server                                                   */
/* ------------------------------------------------------------------ */

static cJSON *soundsense_server_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "查询局域网 SoundSense 检测服务器的信息。query=stats 返回服务器统计"
        "（模型/GPU/会话数），query=events 返回服务器侧完整事件历史。"
        "用户想了解检测服务器运行状态或完整历史记录时调用。");
    mcp_tool_info_add_property(tool, "query", "查询类型：stats（默认）或 events", "string", false);
    return tool;
}

/* minimal HTTP GET to fetch server /healthz or /v1/stats (R3) */
static char *ss_fetch_server(const char *path)
{
    /* use lisa_http if available, else return NULL */
    /* for now return a stub that constructs from cached status */
    return NULL;
}

static cJSON *soundsense_server_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    const cJSON *q = mcp_tool_call_args_get(args, "query");
    const char *query = cJSON_IsString(q) ? q->valuestring : "stats";

    /* The server's R2/R3 endpoints are not yet implemented; fall back to
     * echoing what the device knows about the server connection */
    ss_status_t st;
    ss_core_get_status(&st);

    char text[256];
    if (strcmp(query, "events") == 0) {
        snprintf(text, sizeof(text),
                 "服务器事件查询接口(/v1/events)尚未在服务端实现（R2）。\n"
                 "当前设备侧缓存的检测事件可通过 soundsense_events 工具查询。");
    } else {
        snprintf(text, sizeof(text),
                 "SoundSense 服务器信息（设备视角）：\n"
                 "  地址：%s\n"
                 "  连接状态：%s\n"
                 "  累计推理结果：%u条\n"
                 "  累计事件：%u个\n"
                 "  重连次数：%u\n"
                 "  （服务端 /v1/stats 接口尚未实现 R3，以上为设备侧统计数据）",
                 st.server_url, k_state_names[st.state],
                 st.result_count, st.event_count, st.reconnect_count);
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) return NULL;
    cJSON_AddStringToObject(result, "content", text);
    cJSON_AddBoolToObject(result, "isError", false);
    return result;
}

/* ------------------------------------------------------------------ */
/* registration                                                        */
/* ------------------------------------------------------------------ */

MCP_TOOL_DEFINE(soundsense_status, soundsense_status_list, soundsense_status_call);
MCP_TOOL_DEFINE(soundsense_events, soundsense_events_list, soundsense_events_call);
MCP_TOOL_DEFINE(soundsense_server, soundsense_server_list, soundsense_server_call);
