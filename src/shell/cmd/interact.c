#include "shell.h"
#include "stdint.h"
#include "string.h"
#include "cmd.h"

#define TAG "shell-interact"
#include "voice_msg.h"
#include "lisa_log.h"

static int cmd_interact_start(int argc, char **argv)
{
    LOGI("interact start (simulating click wakeup)");
    const char keyword[] = "xiao ling xiao ling";
    voice_msg_pub(VOICE_MSG_WAKEUP_KEYWORD, (void *)keyword, sizeof(keyword));
    return 0;
}

static int cmd_interact_stop(int argc, char **argv)
{
    LOGI("interact stop (exit cloud session)");
    voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
    return 0;
}

static int cmd_interact_help(int argc, char **argv);

static const struct listen_cmd_t g_interact_cmds[] = {
    {"start", cmd_interact_start, "Enter interaction mode, ex: interact start"},
    {"stop", cmd_interact_stop, "Exit interaction mode, ex: interact stop"},
    {"help", cmd_interact_help, "Show help information"},
};

static int cmd_interact_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_interact_cmds) / sizeof(g_interact_cmds[0]);
    shellPrint(shellGetCurrent(), "Interaction mode commands:\n");
    for (int i = 0; i < cmd_len; i++) {
        if (g_interact_cmds[i].help != NULL) {
            shellPrint(shellGetCurrent(), "%-17s\t:\t%s\n", g_interact_cmds[i].name, g_interact_cmds[i].help);
        }
    }
    return 0;
}

static int interact_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        cmd_interact_help(argc, argv);
        return 0;
    }

    for (int i = 0; i < sizeof(g_interact_cmds) / sizeof(g_interact_cmds[0]); i++) {
        if (strcmp(g_interact_cmds[i].name, argv[1]) == 0) {
            return g_interact_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    cmd_interact_help(argc, argv);
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 interact, interact_cmd_handler, interaction mode commands);
