#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "voice_msg.h"
#include "sysheap.h"

#include "mcp.h"
#include "cJSON.h"

struct emoji_info {
    char *name;
    char *name_cn;
};

static const struct emoji_info emoji_list[] = {
    {"angry", "生气"},
    {"happy", "开心"},
    {"cute", "撒娇"},
    {"neutral", "无表情"},
    {"sad", "悲伤"},
    {"laugh", "大笑"},
};

static cJSON *set_emotion_list(const char *name)
{
    char *description = psram_malloc(256);
    char *property_desc = psram_malloc(512);
    char *final_prop_desc = psram_malloc(600);
    cJSON *tool = NULL;

    if (!description || !property_desc || !final_prop_desc) {
        LOGE("Failed to allocate memory for emotion list");
        goto cleanup;
    }

    memset(description, 0, 256);
    memset(property_desc, 0, 512);
    int desc_len = 0;
    int prop_len = 0;

    for (int i = 0; i < sizeof(emoji_list) / sizeof(emoji_list[0]); i++) {
        if (i > 0) {
            desc_len += snprintf(description + desc_len, 256 - desc_len, ", ");
            prop_len += snprintf(property_desc + prop_len, 512 - prop_len, "、");
        }
        desc_len += snprintf(description + desc_len, 256 - desc_len, "%s", emoji_list[i].name);
        prop_len += snprintf(property_desc + prop_len, 512 - prop_len, "'%s'/'%s'",
                            emoji_list[i].name_cn, emoji_list[i].name);
    }

    snprintf(final_prop_desc, 600, "表情类型，可以是 %s", property_desc);

    tool = mcp_tool_list_info_create_default(name, description);
    if (!tool) {
        goto cleanup;
    }

    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        cJSON_Delete(tool);
        tool = NULL;
        goto cleanup;
    }

    mcp_tool_info_add_property(tool, "emotion", final_prop_desc, "string", true);

cleanup:
    if (description) {
        psram_free(description);
    }
    if (property_desc) {
        psram_free(property_desc);
    }
    if (final_prop_desc) {
        psram_free(final_prop_desc);
    }

    return tool;
}

static cJSON *set_emotion_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *emotion = mcp_tool_call_args_get(args, "emotion");
    if (!emotion || emotion->valuestring == NULL) {
        LOGE("mcp tool call args get emotion failed");
        return NULL;
    }

    LOGI("mcp emoji received, name: %s", emotion->valuestring);

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON *content_array = cJSON_CreateArray();
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "text");
    cJSON_AddStringToObject(content_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, content_item);
    cJSON_AddItemToObject(result, "content", content_array);
    cJSON_AddBoolToObject(result, "isError", false);

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_EMOJI, emotion->valuestring, strlen(emotion->valuestring) + 1);

    return result;
}

MCP_TOOL_DEFINE(ls.built_in.set_emotion, set_emotion_list, set_emotion_call);
