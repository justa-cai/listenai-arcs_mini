#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "app_datas.h"
#include "kv_user.h"
#include "lisa_kv.h"
#include "voice_cloud.h"

#include "apps/llm/models/model_voice.h"
#include "voice_msg.h"


#define TAG "mcp_tool_duplex"

static cJSON *duplex_switch_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name,
        "该工具用于切换交互模式，在[单工模式/半双工模式/单轮对话] 与 [全双工模式/连续对话/多轮对话] 这两种对话模式之间切换");
    if (!tool) {
        return NULL;
    }

    mcp_tool_info_add_property(tool, "value","开关，true为进入[全双工模式/连续对话/多轮对话]，false为[单工模式/半双工模式/单轮对话]","boolean", true);

    return tool;
}

static cJSON *duplex_switch_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *value_json = mcp_tool_call_args_get(args, "value");
    if (!value_json || !cJSON_IsBool(value_json)) {
        LOGE("value parameter not found or invalid");
        return NULL;
    }

    bool enable_full_duplex = cJSON_IsTrue(value_json);
    LOGI("switch duplex -> %s", enable_full_duplex ? "full" : "half");


    model_voice_wakeup_mode_t mode = enable_full_duplex ? MODEL_VOICE_WAKEUP_MODE_VOICE_MULTI : MODEL_VOICE_WAKEUP_MODE_VOICE_SINGLE;
    if (model_voice_wakeup_mode_set(mode) != 0) {
        LOGW("model_voice_wakeup_mode_set failed, fallback to app_datas only");
    }

    struct app_datas *app_datas = get_app_datas();
    if (app_datas) {
        app_datas->full_duplex = enable_full_duplex;
        app_datas->voice_work_mode &= ~VOICE_WORK_MODE_BUTTON_WAKEUP;
        app_datas->voice_work_mode |= VOICE_WORK_MODE_VOICE_WAKEUP;
        lisa_kv_set_bool(KV_KEY_FULL_DUPLEX, app_datas->full_duplex);
        lisa_kv_set_int(KV_KEY_WAKEUP_MODE, (int)app_datas->voice_work_mode);
    }

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
    
    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    char text[96];
    const char *mode_desc = enable_full_duplex ? "全双工模式" : "单工模式";
    snprintf(text, sizeof(text), "已切换到%s", mode_desc);

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", text);
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.switch_full_duplex, duplex_switch_list, duplex_switch_call);
