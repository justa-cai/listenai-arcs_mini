/*
 * offline_cammand.c - offline voice command words for the virtual pet
 *
 * Flow (see docs2.listenai.com/x/MdsBamBa1):
 *   wake word -> ESR command window (acomp_wakeup_set_timeout) ->
 *   VOICE_MSG_WAKEUP_COMMAND(keyword) -> rcmd_router (online-first) ->
 *   VOICE_MSG_RECOGNIZED_OFFLINE_COMMAND(voice_msg_cloud_recognized_command_t)
 *
 * This module matches the recognized keyword against the pet command table
 * and drives pet_core_action(). The command-word resource (cae_esr.bin +
 * wrap.json) is generated on the LUNA algorithm platform with exactly these
 * keywords. While the cloud is connected the router drops offline commands
 * and the cloud LLM reaches the same actions through the pet_care MCP tool.
 */
#include <stdio.h>
#include <string.h>

#include "lisa_log.h"
#include "lisa_ui_invoke.h"
#include "lisa_ui_nav_scr.h"
#include "lisa_ui_nav_scr_ids.h"
#include "sys_init.h"
#include "voice_msg.h"

#include "pet/pet_core.h"
#include "pet/pet_ui.h"

#define TAG "offline.cmd"

typedef enum {
    PET_CMD_ACTION = 0,
    PET_CMD_NAV_PET,
    PET_CMD_NAV_HOME,
} pet_cmd_kind_t;

typedef struct {
    const char *keyword;
    uint8_t kind;
    uint8_t action; /* pet_action_t */
    uint8_t item;   /* pet_item_t */
} pet_cmd_t;

/* Keep in sync with the command-word resource generated on the platform. */
static const pet_cmd_t k_pet_cmds[] = {
    {"喂它吃饭", PET_CMD_ACTION, PET_ACTION_FEED, PET_ITEM_MEAL},
    {"给它零食", PET_CMD_ACTION, PET_ACTION_FEED, PET_ITEM_SNACK},
    {"打扫卫生", PET_CMD_ACTION, PET_ACTION_CLEAN, PET_ITEM_MEAL},
    {"陪它玩耍", PET_CMD_ACTION, PET_ACTION_PLAY, PET_ITEM_MEAL},
    {"喂它吃药", PET_CMD_ACTION, PET_ACTION_MEDICINE, PET_ITEM_MEAL},
    {"开灯", PET_CMD_ACTION, PET_ACTION_LIGHT, PET_ITEM_LIGHT_WAKE},
    {"关灯", PET_CMD_ACTION, PET_ACTION_LIGHT, PET_ITEM_LIGHT_SLEEP},
    {"它还好吗", PET_CMD_ACTION, PET_ACTION_STATUS, PET_ITEM_MEAL},
    {"打开宠物", PET_CMD_NAV_PET, 0, 0},
    {"回到主页", PET_CMD_NAV_HOME, 0, 0},
    {"你好呀", PET_CMD_ACTION, PET_ACTION_GREET, PET_ITEM_MEAL},
    {"我爱你", PET_CMD_ACTION, PET_ACTION_LOVE, PET_ITEM_MEAL},
    {"摸摸它", PET_CMD_ACTION, PET_ACTION_PAT, PET_ITEM_MEAL},
};

/* latest reply text for the UI-thread toast (commands are serialized on the
 * ebus thread, a single static buffer is enough) */
static char s_tts_text[96];

static void pet_cmd_handle(const pet_cmd_t *cmd, const char *keyword)
{
    if (cmd->kind == PET_CMD_NAV_PET || cmd->kind == PET_CMD_NAV_HOME) {
        uint32_t target = (cmd->kind == PET_CMD_NAV_PET) ? LISA_UI_NAV_SCR_ID_PET
                                                         : LISA_UI_NAV_SCR_ID_HOME;
        LISA_UI_INVOKE_UI_ARG_BASE(target, {
            (void)lisa_ui_nav_scr_nav_to(_invoke_target);
        });
        LISA_LOGI(TAG, "'%s' -> nav %s", keyword, cmd->kind == PET_CMD_NAV_PET ? "pet" : "home");
        return;
    }

    pet_result_t result;
    pet_core_action(cmd->action, cmd->item, &result);
    LISA_LOGI(TAG, "'%s' -> action %u/%u: %s", keyword, cmd->action, cmd->item, result.tts);

    /* the pet page toasts the reaction events itself within ~1 s, but a
     * direct toast makes the voice feedback feel immediate */
    if (result.tts[0] != '\0') {
        snprintf(s_tts_text, sizeof(s_tts_text), "%s", result.tts);
        LISA_UI_INVOKE_UI_ARG_NONE({ pet_ui_toast(s_tts_text); });
    }
}

static void pet_offline_command_cb(void *unused, uint32_t msg_id, void *data, uint32_t len,
                                   void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (!data || len < sizeof(voice_msg_cloud_recognized_command_t)) {
        return;
    }
    voice_msg_cloud_recognized_command_t *cmd_msg = data;
    if (cmd_msg->command[0] == '\0') {
        return;
    }

    for (size_t i = 0; i < sizeof(k_pet_cmds) / sizeof(k_pet_cmds[0]); i++) {
        if (strcmp(cmd_msg->command, k_pet_cmds[i].keyword) == 0) {
            pet_cmd_handle(&k_pet_cmds[i], cmd_msg->command);
            return;
        }
    }
    LISA_LOGI(TAG, "unhandled offline command: '%s'", cmd_msg->command);
}

static int offline_cammand_init(void)
{
    if (voice_msg_sub(VOICE_MSG_RECOGNIZED_OFFLINE_COMMAND, pet_offline_command_cb, NULL) != 0) {
        LISA_LOGE(TAG, "subscribe failed");
        return -1;
    }
    LISA_LOGI(TAG, "pet offline command words ready (%u keywords)",
              (unsigned)(sizeof(k_pet_cmds) / sizeof(k_pet_cmds[0])));
    return 0;
}
SYS_INIT(offline_cammand_init, SYS_INIT_LEVEL_PRE_APPLICATION, 95);
