/*
 * mcp_tool_pet.c - voice care for the Tamagotchi virtual pet (pet_care)
 *
 * The cloud LLM picks this tool when the user talks about feeding, cleaning,
 * playing with, medicating or checking the on-device pet. Actions map onto
 * the emulated three-button UI; the emulator walks the button macro on its
 * own thread and returns a short TTS-friendly reply.
 */
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"

#include "pet/pet_core.h"
#include "pet/pet_ui.h"
#include "pet/pet_voice.h"

#define TAG "pet"

static cJSON *pet_care_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(
        name,
        "电子宠物（拓麻歌子）照料。当用户提到宠物相关的动作时调用：喂食/喂饭/喂零食(feed)、"
        "打扫/清理/铲屎(clean)、陪玩/玩游戏(play)、开灯或关灯(light，切换开关)、喂药/吃药("
        "medicine)、看看宠物状态/它怎么样了(status)、以及通用的切换选项(next)、确认(confirm)、"
        "取消(cancel)。喂食时可用 item 选择 主食(meal，默认) 或 零食(snack)。"
        "设备屏幕会同步显示宠物的反馈动画。");

    cJSON *action_property = cJSON_CreateObject();
    if (!action_property) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddStringToObject(action_property, "type", "string");
    cJSON *action_enum = cJSON_CreateArray();
    if (!action_enum) {
        cJSON_Delete(action_property);
        cJSON_Delete(tool);
        return NULL;
    }
    const char *actions[] = {"feed", "clean", "play", "light", "medicine", "status", "next", "confirm", "cancel"};
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        cJSON *item = cJSON_CreateString(actions[i]);
        if (!item || !cJSON_AddItemToArray(action_enum, item)) {
            cJSON_Delete(action_enum);
            cJSON_Delete(action_property);
            cJSON_Delete(tool);
            return NULL;
        }
    }
    cJSON_AddItemToObject(action_property, "enum", action_enum);
    cJSON_AddStringToObject(action_property, "description",
                            "照料动作：feed(喂食)、clean(打扫)、play(陪玩游戏)、light(开/关灯切换)、"
                            "medicine(喂药)、status(查看状态)、next(切换选中图标)、confirm(确认)、"
                            "cancel(取消)。");
    mcp_tool_info_add_json_property(tool, "action", action_property, true);

    cJSON *item_property = cJSON_CreateObject();
    if (!item_property) {
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddStringToObject(item_property, "type", "string");
    cJSON *item_enum = cJSON_CreateArray();
    if (!item_enum) {
        cJSON_Delete(item_property);
        cJSON_Delete(tool);
        return NULL;
    }
    cJSON_AddItemToArray(item_enum, cJSON_CreateString("meal"));
    cJSON_AddItemToArray(item_enum, cJSON_CreateString("snack"));
    cJSON_AddItemToObject(item_property, "enum", item_enum);
    cJSON_AddStringToObject(item_property, "description",
                            "喂食的内容：meal(主食，默认) 或 snack(零食)。仅 action=feed 时有效。");
    mcp_tool_info_add_json_property(tool, "item", item_property, false);

    return tool;
}

static int pet_care_action_parse(const char *text)
{
    if (!text) {
        return -1;
    }
    if (strcmp(text, "feed") == 0) return PET_ACTION_FEED;
    if (strcmp(text, "clean") == 0) return PET_ACTION_CLEAN;
    if (strcmp(text, "play") == 0) return PET_ACTION_PLAY;
    if (strcmp(text, "light") == 0) return PET_ACTION_LIGHT;
    if (strcmp(text, "medicine") == 0) return PET_ACTION_MEDICINE;
    if (strcmp(text, "status") == 0) return PET_ACTION_STATUS;
    if (strcmp(text, "next") == 0) return PET_ACTION_NEXT;
    if (strcmp(text, "confirm") == 0) return PET_ACTION_CONFIRM;
    if (strcmp(text, "cancel") == 0) return PET_ACTION_CANCEL;
    return -1;
}

static cJSON *pet_care_call(const char *id, const char *name, cJSON *args)
{
    (void)id;

    char reply[96] = {0};
    bool is_error = true;
    int action = -1;
    int item = PET_ITEM_MEAL;

    const cJSON *action_json = mcp_tool_call_args_get(args, "action");
    if (!cJSON_IsString(action_json) || action_json->valuestring == NULL) {
        snprintf(reply, sizeof(reply), "缺少有效的 action 参数");
        goto out;
    }
    action = pet_care_action_parse(action_json->valuestring);
    if (action < 0) {
        snprintf(reply, sizeof(reply), "未知的 action: %s", action_json->valuestring);
        goto out;
    }

    item = PET_ITEM_MEAL;
    const cJSON *item_json = mcp_tool_call_args_get(args, "item");
    if (cJSON_IsString(item_json) && item_json->valuestring != NULL) {
        if (strcmp(item_json->valuestring, "snack") == 0) {
            item = PET_ITEM_SNACK;
        } else if (strcmp(item_json->valuestring, "meal") != 0) {
            snprintf(reply, sizeof(reply), "未知的 item: %s", item_json->valuestring);
            goto out;
        }
    }

    pet_result_t result;
    int rc = pet_core_action((uint8_t)action, (uint8_t)item, &result);
    snprintf(reply, sizeof(reply), "%s", result.tts);
    is_error = (rc != 0);

    /* the pet's own line plays on-device too (tone channel captures tts,
     * interrupting the cloud answer by design); the cloud reads the
     * dynamic text (with live numbers) for the conversational reply */
    pet_voice_play(result.tone_id);
    /* refresh the screen (animation/toast) right away instead of waiting
     * for the 1 s UI poll */
    pet_ui_kick();

out:;
    cJSON *result_json = mcp_tool_call_result_create(name);
    if (!result_json) {
        return NULL;
    }
    cJSON_AddStringToObject(result_json, "content", reply);
    cJSON_AddBoolToObject(result_json, "isError", is_error);
    LISA_LOGI(TAG, "care: action=%d item=%d -> %s", action >= 0 ? action : -1, item, reply);
    return result_json;
}

MCP_TOOL_DEFINE(pet_care, pet_care_list, pet_care_call);
