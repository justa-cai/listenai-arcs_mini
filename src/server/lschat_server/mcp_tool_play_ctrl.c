#include <stdbool.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "voice_msg.h"

static cJSON *ls_playback_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name, "播放控制工具：用于控制播放器的相关功能，如'继续播放'、'暂停'、'上一个'、'下一个'、'重播'");
    if (!tool) {
        return NULL;
    }

    cJSON *properties = mcp_tool_info_properties_get(tool);
    if (!properties) {
        cJSON_Delete(tool);
        return NULL;
    }

    mcp_tool_info_add_property(tool, "intent",
                               "具体的指令："
                               "RESUME_PLAY（播放、继续、取消暂停等继续播放意图）、"
                               "PAUSE（暂停、不想听了等暂停播放意图）、"
                               "CHOOSE_PREVIOUS（上一个、前一首等向上意图）、"
                               "CHOOSE_NEXT（下一个、切歌等向下意图）、"
                               "REPLAY（重播、再唱一遍等重复播放意图）",
                               "string", true);

    return tool;
}

static cJSON *ls_playback_control_call(const char *id, const char *name, cJSON *args)
{
    const cJSON *intent = mcp_tool_call_args_get(args, "intent");
    if (!intent) {
        LOGE("mcp tool call args get intent failed");
        return NULL;
    }

    if (strcmp(cJSON_GetStringValue(intent), "RESUME_PLAY") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_PLAY, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "PAUSE") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_PAUSE, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "CHOOSE_PREVIOUS") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "CHOOSE_NEXT") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_NEXT, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "REPLAY") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_REPLAY, NULL, 0);
    } else {
        LOGE("invalid intent: %s", cJSON_GetStringValue(intent));
        return NULL;
    }

    cJSON *result = mcp_tool_call_result_create(name);
    if (!result) {
        return NULL;
    }

    cJSON_AddStringToObject(result, "content", "执行成功");

    return result;
}

MCP_TOOL_DEFINE(ls.playback_control, ls_playback_control_list, ls_playback_control_call);
