#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "jk_cloud.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "evs_utils.h"

#define TAG "shell-tts"

typedef struct {
    char *text;
} tts_cmd_arg_t;

static int _handle_cmd_tts_request(void *arg)
{
    tts_cmd_arg_t *tts_arg = (tts_cmd_arg_t *)arg;
    if (tts_arg && tts_arg->text) {
        printf("TTS request: %s\n", tts_arg->text);
        jk_cloud_tts(tts_arg->text);
        lisa_mem_free(tts_arg->text);
        lisa_mem_free(tts_arg);
    }

    return 0;
}

static int cmd_tts_request(int argc, char **argv)
{
    if (argc < 1) {
        printf("Usage: tts <text>\n");
        printf("Example: tts 你好\n");
        return -1;
    }

    // Concatenate all arguments into a single string
    size_t total_len = 0;
    for (int i = 0; i < argc; i++) {
        total_len += strlen(argv[i]) + 1;  // +1 for space or null terminator
    }

    char *text = lisa_mem_alloc(total_len);
    if (!text) {
        printf("Failed to allocate memory for TTS text\n");
        return -1;
    }

    text[0] = '\0';
    for (int i = 0; i < argc; i++) {
        strcat(text, argv[i]);
        if (i < argc - 1) {
            strcat(text, " ");
        }
    }

    printf("TTS command: %s\n", text);

    tts_cmd_arg_t *arg = lisa_mem_alloc(sizeof(tts_cmd_arg_t));
    if (!arg) {
        lisa_mem_free(text);
        printf("Failed to allocate memory for TTS argument\n");
        return -1;
    }
    arg->text = text;

    if (evs_handler_post_runnable(_handle_cmd_tts_request, arg) != 0) {
        lisa_mem_free(text);
        lisa_mem_free(arg);
        printf("Failed to post TTS request to event loop\n");
        return -1;
    }

    return 0;
}

static int cmd_tts_help(int argc, char **argv);
static const struct listen_cmd_t g_tts_cmds[] = {
    {"request", cmd_tts_request, "tts request, ex: tts request 你好"},
    {"help", cmd_tts_help, NULL},
};

static int cmd_tts_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_tts_cmds) / sizeof(g_tts_cmds[0]);
    for (int i = 0; i < cmd_len; i++) {
        if (g_tts_cmds[i].help != NULL && strcmp(g_tts_cmds[i].name, "help") != 0) {
            printf("%-16s\t:\t%s\n", g_tts_cmds[i].name, g_tts_cmds[i].help);
        }
    }

    return 0;
}

static int tts_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        // Direct usage: tts <text>
        if (argc > 1) {
            return cmd_tts_request(argc - 1, argv + 1);
        } else {
            cmd_tts_help(argc, argv);
            return 0;
        }
    }

    // Check if second argument is a subcommand
    for (int i = 0; i < sizeof(g_tts_cmds) / sizeof(g_tts_cmds[0]); i++) {
        if (strcmp(g_tts_cmds[i].name, argv[1]) == 0) {
            return g_tts_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    // If no subcommand matched, treat all arguments as text
    return cmd_tts_request(argc - 1, argv + 1);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, tts,
                 tts_cmd_handler, TTS text-to-speech command);
