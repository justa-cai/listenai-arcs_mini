#include "stdio.h"
#include "stdlib.h"
#include "stddef.h"
#include "string.h"
#include "lisa_log.h"
#include "shell.h"
#include "app_shell.h"

struct listen_cmd_t {
    char *name;
    int (*exec)(int argc, char **argv);
    char *help;
};

static int shell_hello_world(int argc, char **argv)
{
    printf("Hello World\n");
    return 0;
}

static int shell_echo(int argc, char **argv)
{
    if (argc < 1) {
        printf("Usage: echo <message>\n");
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    return 0;
}

static const struct listen_cmd_t g_shell_cmds[] = {
    {"helloworld", shell_hello_world, "Print Hello World"},
    {"echo", shell_echo, "Echo command"},
};

static int shell_test_cmd_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_shell_cmds) / sizeof(g_shell_cmds[0]);
    for (int i = 0; i < cmd_len; i++) {
        if (g_shell_cmds[i].help != NULL) {
            printf("%-17s\t:\t%s\n", g_shell_cmds[i].name, g_shell_cmds[i].help);
        }
    }

    return 0;
}

static int shell_test_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        shell_test_cmd_help(argc, argv);
        return 0;
    }

    int i;

    for (i = 0; i < sizeof(g_shell_cmds) / sizeof(g_shell_cmds[0]); i++) {
        if (strcmp(g_shell_cmds[i].name, argv[1]) == 0) {
            return g_shell_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    shell_test_cmd_help(argc, argv);

    return 0;
}

static void log_shell_backend_output(const uint8_t *log, uint32_t len, void *data)
{
    lisa_shell_output_raw((const char *)log, len);
}

int main(int argc, char **argv)
{
    LOGI("UART shell sample\n");

    lisa_shell_init();

    /* shell的串口和系统默认的日志输出串口是同一个, 这里暂停系统默认的日志输出 */
    lisa_log_backend_pause("sys.log");
    lisa_log_backend_add("user.shell", log_shell_backend_output, NULL);

    while (1) {
        LOGI("shell test is running\n");
        vTaskDelay(1000);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, shell_test,
                 shell_test_cmd_handler, shell test cmd group);
