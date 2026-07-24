#include <stdbool.h>

#include "cJSON.h"
#include "lisa_log.h"

#include "mcp.h"
#include "voice_msg.h"

static cJSON *ls_playback_control_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name, "用于控制当前播放器，仅处理明确的播放控制指令，包括暂停、恢复播放、上一首、下一首、重播和停止。强制调用顺序：首次响应必须先输出非空自然语言“我来处理播放控制。”，工具调用必须作为后续事件发送；禁止首事件直接调用工具，也禁止调用后再补前置文本。用户明确说“继续播放、恢复播放、取消暂停”时必须调用本工具并使用 RESUME_PLAY，不得改用 play_music。若用户整句只有“继续、继续吧、接着、接着来”等裸词，且上下文没有正在播放或暂停的媒体，则不要调用本工具，应交由 play_music 处理。即使是“下一首、上一首、重播、停止、继续播放”等短指令，也必须先输出前置文本再调用本工具。");
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
                               "REPLAY（重播、再唱一遍等重复播放意图）、"
                               "STOP（停止、别放了等停止播放意图）",
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
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "CHOOSE_PREVIOUS") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "CHOOSE_NEXT") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_NEXT, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "REPLAY") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_REPLAY, NULL, 0);
    } else if (strcmp(cJSON_GetStringValue(intent), "STOP") == 0) {
        voice_msg_pub(VOICE_MSG_PLAY_CONTROL_STOP, NULL, 0);
        voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
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
