#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "lisa_kv.h"
#include "lisa_mem.h"

#include "shell.h"

#include "stdint.h"
#include "stdio.h"
#include "alarm_store.h"

static int kv_cmd_del(int argc, char **argv)
{
    if (argc > 0) {
        char *key = argv[0];

        if (lisa_kv_del(key) == 0) {
            shellPrint(shellGetCurrent(),"flash delete config: %s, success\n", key);
        } else {
            shellPrint(shellGetCurrent(),"flash has no config: %s\n", key);
        }
    } else {
        shellPrint(shellGetCurrent(),"flash clear no arg\n");
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
        shellPrint(shellGetCurrent(),"invalid index %d\n", argc);
        return -1;
    } else {
        char *type = argv[0];
        char *key = argv[1];
        char *value = argv[2];
        if (!strcmp(type, "string")) {
            if (lisa_kv_set_string(key, value) != 0) {
                shellPrint(shellGetCurrent(),"flash set %s:%s failed\n", key, value);
            } else {
                shellPrint(shellGetCurrent(),"flash set %s:%s success\n", key, value);
            }
        } else if (!strcmp(type, "int")) {
            int int_temp = atoi(value);

            // Validate range for mic_gain and aec_gain (0-100)
            if (!strcmp(key, "user.mic_gain") || !strcmp(key, "user.aec_gain") || !strcmp(key, "user.volume") ) {
                if (int_temp < 0 || int_temp > 100) {
                    shellPrint(shellGetCurrent(),"[ERROR] Invalid value for %s: %d (valid range: 0-100), not modified\n", key, int_temp);
                    return -1;
                }
            }
            
            if (lisa_kv_set_int(key, int_temp) != 0) {
                shellPrint(shellGetCurrent(),"flash set %s:%d failed\n", key, int_temp);
            } else {
                shellPrint(shellGetCurrent(),"flash set %s:%d success\n", key, int_temp);
                // DEBUG: 调试麦克风/AEC增益时自动调用debug函数
                if (!strcmp(key, "user.mic_gain") || !strcmp(key, "user.aec_gain") || !strcmp(key, "user.volume")) {
                    extern void listen_mic_gain_set_debug(void);
                    listen_mic_gain_set_debug();
                }
                // Update interactive mode when user.intmode is set
                if (!strcmp(key, "user.intmode")) {
                    extern int lisa_aiui_set_interactive_mode(int mode);
                    lisa_aiui_set_interactive_mode(int_temp);
                    enter_audio_idle();
                }
            }
        } else if (!strcmp(type, "bool")) {
            int bool_temp = atoi(value);
            if (lisa_kv_set_bool(key, bool_temp) != 0) {
                shellPrint(shellGetCurrent(),"flash set %s:%d failed\n", key, bool_temp);
            } else {
                shellPrint(shellGetCurrent(),"flash set %s:%d success\n", key, bool_temp);
            }
        } else {
            shellPrint(shellGetCurrent(),"invalid type %s\n", type);
            return -1;
        }
    }
    return 0;
}

static int kv_cmd_get(int argc, char **argv)
{
    if (argc < 1) {
        shellPrint(shellGetCurrent(),"invalid index %d\n", argc);
        return -1;
    } else {
        char *type = argv[0];
        char *key = argv[1];
        if (!strcmp(type, "string")) {
            char *str_value = NULL;
            if (lisa_kv_get_string(key, &str_value) != 0) {
                shellPrint(shellGetCurrent(),"flash get %s failed\n", key);
            } else {
                shellPrint(shellGetCurrent(), "flash get %s:", key);
                int value_len = strlen(str_value);
                for (int i = 0; i < value_len; i += SHELL_PRINT_BUFFER) {
                    int chunk_len = (value_len - i >= SHELL_PRINT_BUFFER) ? 
                                    SHELL_PRINT_BUFFER : (value_len - i);
                    shellPrint(shellGetCurrent(), "%.*s", chunk_len, str_value + i);
                }
                shellPrint(shellGetCurrent(), " success\n");
            }
            if (NULL != str_value) {
                lisa_mem_free(str_value);
            }
        } else if (!strcmp(type, "int")) {
            int int_value = 0;
            if (lisa_kv_get_int(key, &int_value) != 0) {
                shellPrint(shellGetCurrent(),"flash get %s failed\n", key);
            } else {
                shellPrint(shellGetCurrent(),"flash get %s:%d success\n", key, int_value);
            }
        } else if (!strcmp(type, "bool")) {
            bool bool_value = 0;
            if (lisa_kv_get_bool(key, &bool_value) != 0) {
                shellPrint(shellGetCurrent(),"flash get %s failed\n", key);
            } else {
                shellPrint(shellGetCurrent(),"flash get %s:%d success\n", key, bool_value);
            }
        } else if (!strcmp(type, "blob")) {
            uint8_t *blob = NULL;
            int blob_len = 0;
            if (lisa_kv_get_blob(key, &blob, &blob_len) != 0 || blob == NULL || blob_len <= 0) {
                shellPrint(shellGetCurrent(),"flash get %s failed\n", key);
            } else {
                shellPrint(shellGetCurrent(), "flash get %s len:%d\n", key, blob_len);

                /* Special handling: user.alarm.list is a uint64 list of alarm timestamps */
                if (!strcmp(key, "user.alarm_list") || !strcmp(key, "user.alarm.keys")) {
                    int cnt = blob_len / (int)sizeof(uint64_t);
                    uint64_t *ts = (uint64_t *)blob;
                    for (int i = 0; i < cnt; i++) {
                        shellPrint(shellGetCurrent(), "  alarm[%d]: %llu\n", i, (unsigned long long)ts[i]);
                    }
                } else {
                    // Try to print as alarm_obj_nvs_t if size matches
                    if (blob_len >= sizeof(alarm_obj_nvs_t)) {
                        alarm_obj_nvs_t *hdr = (alarm_obj_nvs_t *)blob;
                        size_t text_len = hdr->text_len;
                        if (blob_len >= sizeof(alarm_obj_nvs_t) + text_len) {
                            alarm_object_t obj;
                            extern void alarm_object_from_nvs(const alarm_obj_nvs_t *hdr, const uint8_t *text, size_t text_len, uint64_t alarm_id, alarm_object_t *out_alarm);
                            alarm_object_from_nvs(hdr, blob + sizeof(alarm_obj_nvs_t), text_len, 0, &obj);
                            alarm_obj_print(&obj);
                        } else {
                            shellPrint(shellGetCurrent(), "[alarm_obj_nvs_t blob too short for text] \n");
                        }
                    } else {
                        /* Generic hex dump in chunks to avoid oversized prints */
                        for (int i = 0; i < blob_len; i++) {
                            shellPrint(shellGetCurrent(), "%hhd", blob[i]);
                            if ((i + 1) % 32 == 0) {
                                shellPrint(shellGetCurrent(), "\n");
                            } else if ((i + 1) % 2 == 0) {
                                shellPrint(shellGetCurrent(), " ");
                            }
                        }
                        shellPrint(shellGetCurrent(), "\n");
                    }
                }
            }
            if (blob) {
                lisa_mem_free(blob);
            }
        } else {
            shellPrint(shellGetCurrent(),"invalid type\n");
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
            shellPrint(shellGetCurrent(),"%-17s\t:\t%s\n", g_kv_cmds[i].name, g_kv_cmds[i].help);
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
