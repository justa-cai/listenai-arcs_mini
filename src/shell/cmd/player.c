#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "app_player.h"
#include "lisa_mem.h"
#include "lisa_log.h"

#include "evs_utils.h"

#define TAG "shell-player"

static int __cmd_player_status_cb(uint16_t st)
{
    // LISA_LOGI(TAG, "main player status: %d", st);
    printf("main player status: %d\n", st);

    return 0;
}

static int _handle_cmd_player_play(void *arg)
{
    char *url = (char *)arg;
    if (url) {
        app_player_play(PLAYER_T_TONE, url, __cmd_player_status_cb);
        lisa_mem_free(arg);
    }

    return 0;
}

static int cmd_player_play(int argc, char **argv)
{
    if (argc < 1) {
        printf("invalid index %d\n", argc);
        return -1;
    } else {
        char *url = lisa_mem_alloc(strlen(argv[0]) + 1);
        strcpy(url, argv[0]);
        printf("player play url: %s\n", url);
        if (evs_handler_post_runnable(_handle_cmd_player_play, url) != 0) {
            lisa_mem_free(url);
        }
    }
    return 0;
}

static int _handle_cmd_player_stop()
{
    app_player_stop(PLAYER_T_TONE);
    return 0;
}

static int cmd_player_stop(int argc, char **argv)
{
    evs_handler_post_runnable(_handle_cmd_player_stop, NULL);
    return 0;
}

static int _handle_cmd_player_pause()
{
    app_player_pause(PLAYER_T_TONE);
    return 0;
}

static int cmd_player_pause(int argc, char **argv)
{
    evs_handler_post_runnable(_handle_cmd_player_pause, NULL);
    return 0;
}

static int _handle_cmd_player_resume()
{
    app_player_resume_sync(PLAYER_T_TONE);
    return 0;
}

static int cmd_player_resume(int argc, char **argv)
{
    evs_handler_post_runnable(_handle_cmd_player_resume, NULL);
    return 0;
}

static int cmd_player_setloglev(int argc, char **argv)
{
    if (argc < 1) {
        printf("invalid index %d\n", argc);
        return -1;
    } else {
        int lev = atoi(argv[0]);
        if (lev >= 0 && lev <= 5) {
            printf("set player log level %d\n", lev);
            lisa_log_set_level(lev);
        } else {
            printf("invalid log level %d\n", lev);
        }
    }
    return 0;
}

static int cmd_player_help(int argc, char **argv);
static const struct listen_cmd_t g_player_cmds[] = {
    {"play", cmd_player_play, "player play, ex: listen player play http://***"},
    {"stop", cmd_player_stop, "player stop, ex: listen player stop"},
    {"pause", cmd_player_pause, "player pause, ex: listen player pause"},
    {"resume", cmd_player_resume, "player resume, ex: listen player resume"},
    {"help", cmd_player_help, NULL},
};

static int cmd_player_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_player_cmds) / sizeof(g_player_cmds[0]);
    for (int i = 0; i < cmd_len; i++) {
        if (g_player_cmds[i].help != NULL && strcmp(g_player_cmds[i].name, "help") != 0) {
            printf("%-16s\t:\t%s\n", g_player_cmds[i].name, g_player_cmds[i].help);
        }
    }

    return 0;
}

static int player_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        cmd_player_help(argc, argv);
        return 0;
    }
    int i;

    for (i = 0; i < sizeof(g_player_cmds) / sizeof(g_player_cmds[0]); i++) {
        if (strcmp(g_player_cmds[i].name, argv[1]) == 0) {
            return g_player_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    cmd_player_help(argc, argv);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, player,
                 player_cmd_handler, player cmd group);
