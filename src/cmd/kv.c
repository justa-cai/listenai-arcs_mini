#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lisa_kv.h"
#include "lisa_mem.h"

#include "shell.h"

#include "stdint.h"
#include "stdio.h"

static int kv_cmd_del(int argc, char **argv)
{
    if (argc > 0) {
        char *key = argv[0];

        if (lisa_kv_del(key) == 0) {
            printf("flash delete config: %s, success\n", key);
        } else {
            printf("flash has no config: %s\n", key);
        }
    } else {
        printf("flash clear no arg\n");
    }
    return 0;
}

static int kv_cmd_show(int argc, char **argv)
{
    lisa_kv_dump();

    return 0;
}

static int kv_cmd_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("invalid index %d\n", argc);
        return -1;
    } else {
        char *type = argv[0];
        char *key = argv[1];
        char *value = argv[2];
        if (!strcmp(type, "string")) {
            if (lisa_kv_set_string(key, value) != 0) {
                printf("flash set %s:%s failed\n", key, value);
            } else {
                printf("flash set %s:%s success\n", key, value);
            }
        } else if (!strcmp(type, "int")) {
            int int_temp = atoi(value);
            if (lisa_kv_set_int(key, int_temp) != 0) {
                printf("flash set %s:%d failed\n", key, int_temp);
            } else {
                printf("flash set %s:%d success\n", key, int_temp);
            }
        } else if (!strcmp(type, "bool")) {
            int bool_temp = atoi(value);
            if (lisa_kv_set_bool(key, bool_temp) != 0) {
                printf("flash set %s:%d failed\n", key, bool_temp);
            } else {
                printf("flash set %s:%d success\n", key, bool_temp);
            }
        } else {
            printf("invalid type %s\n", type);
            return -1;
        }
    }
    return 0;
}

static int kv_cmd_get(int argc, char **argv)
{
    if (argc < 1) {
        printf("invalid index %d\n", argc);
        return -1;
    } else {
        char *type = argv[0];
        char *key = argv[1];
        if (!strcmp(type, "string")) {
            char *str_value = NULL;
            if (lisa_kv_get_string(key, &str_value) != 0) {
                printf("flash get %s failed\n", key);
            } else {
                printf("flash get %s:%s success\n", key, str_value);
            }
            if (NULL != str_value) {
                lisa_mem_free(str_value);
            }
        } else if (!strcmp(type, "int")) {
            int int_value = 0;
            if (lisa_kv_get_int(key, &int_value) != 0) {
                printf("flash get %s failed\n", key);
            } else {
                printf("flash get %s:%d success\n", key, int_value);
            }
        } else if (!strcmp(type, "bool")) {
            bool bool_value = 0;
            if (lisa_kv_get_bool(key, &bool_value) != 0) {
                printf("flash get %s failed\n", key);
            } else {
                printf("flash get %s:%d success\n", key, bool_value);
            }
        } else {
            printf("invalid type\n");
            return -1;
        }
    }

    return 0;
}

static int kv_cmd_clear(int argc, char **argv)
{
    lisa_kv_clear();

    return 0;
}

static int kv_cmd_help(int argc, char **argv);
static const struct listen_cmd_t g_kv_cmds[] = {
    {"clean", kv_cmd_clear, "clear kv item, ex: kv clean"},
    {"del", kv_cmd_del, "delete kv item, ex: kv del [name]"},
    {"show", kv_cmd_show, "show all kv item, ex: kv show"},
    {"set", kv_cmd_set, "set kv (type: int/bool/string/blob), ex: kv set [type] [name] [value]"},
    {"get", kv_cmd_get, "get kv (type: int/bool/string/blob), ex: kv get [type] [name]"},
    {"help", kv_cmd_help, NULL},
};

static int kv_cmd_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_kv_cmds) / sizeof(g_kv_cmds[0]);
    for (int i = 0; i < cmd_len; i++) {
        if (g_kv_cmds[i].help != NULL) {
            printf("%-17s\t:\t%s\n", g_kv_cmds[i].name, g_kv_cmds[i].help);
        }
    }

    return 0;
}

static int kv_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        kv_cmd_help(argc, argv);
        return 0;
    }
    int i;

    for (i = 0; i < sizeof(g_kv_cmds) / sizeof(g_kv_cmds[0]); i++) {
        if (strcmp(g_kv_cmds[i].name, argv[1]) == 0) {
            return g_kv_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    kv_cmd_help(argc, argv);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, kv,
                 kv_cmd_handler, kv cmd group);
