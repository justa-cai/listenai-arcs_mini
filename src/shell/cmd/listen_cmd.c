// #include <stdint.h>
// #include "evs_utils.h"
// #include "lisa_log.h"
// #include "lisa_mem.h"
// #include "cli_main.h"
// #include "app_player.h"
// #include "listen_wifi.h"
// #include "evs_pref.h"

// #define TAG "lscmd"

// #define LS_CMD_MAX_NUM  (10)

// struct listen_cmd_t {
// 	char *name;
// 	int (*exec)(int argc, char **argv);
// 	char *help;
// };

// static int cmd_listen_help(int argc, char **argv);

// /**
//  * @brief 	分割字符串
//  * @param  	src              待分割源字符串
//  * @param  	separator        分隔符
//  * @param  	dest             分割后的字符串数组
//  * @param  	max_num          最大分割字串
//  * @return 	字串数目
//  * @note	分割后的字串需要使用 lisa_mem_free
//  */
// static int _split(const char *src, const char *separator, char **dest, int max_num)
// {
// 	char *pNext;
// 	int count = 0;
// 	const int src_len = strlen(src);
// 	const int separator_len = strlen(separator);

// 	if (src == NULL || src_len == 0) return 0;
// 	if (separator == NULL || separator_len == 0) return 0;

// 	char *tmp = (char *)lisa_mem_alloc((src_len + 1) * sizeof(char));
// 	strcpy(tmp, src);
// 	pNext = (char *)strtok(tmp, separator);
// 	while (pNext != NULL) {
// 		if(count < max_num) {
// 			*(dest + count) = (char *)lisa_mem_alloc(strlen(pNext) + 1);
// 			strcpy(*(dest + count), pNext);
// 			++count;
// 		}
// 		pNext = (char *)strtok(NULL, separator);
// 	}
// 	lisa_mem_free(tmp);
// 	return count;
// }

// static int __cmd_player_status_cb(uint16_t st)
// {
// 	LISA_LOGI(TAG, "main player status: %d", st);
// 	return 0;
// }

// static int _handle_cmd_player_play(void *arg)
// {
//     char *url = (char *)arg;
//     if (url) {
//         app_player_play(PLAYER_T_TONE, url, __cmd_player_status_cb);
//         lisa_mem_free(arg);
//     }

//     return 0;
// }

// static int cmd_player_play(int argc, char **argv)
// {
// 	if (argc < 1) {
// 		printf("invalid index %d\n", argc);
// 		return -1;
// 	} else {
// 		char *url = lisa_mem_alloc(strlen(argv[0]) + 1);
// 		strcpy(url, argv[0]);
// 		printf("player play url: %s\n", url);
// 		if (evs_handler_post_runnable(_handle_cmd_player_play, url) != 0) {
// 			lisa_mem_free(url);
// 		}
// 	}
// 	return 0;
// }

// static int _handle_cmd_player_stop()
// {
// 	app_player_stop(PLAYER_T_TONE);
// 	return 0;
// }

// static int cmd_player_stop(int argc, char **argv)
// {
// 	evs_handler_post_runnable(_handle_cmd_player_stop, NULL);
// 	return 0;
// }

// static int _handle_cmd_player_pause()
// {
// 	app_player_pause(PLAYER_T_TONE);
// 	return 0;
// }

// static int cmd_player_pause(int argc, char **argv)
// {
// 	evs_handler_post_runnable(_handle_cmd_player_pause, NULL);
// 	return 0;
// }

// static int _handle_cmd_player_resume()
// {
// 	app_player_resume_sync(PLAYER_T_TONE);
// 	return 0;
// }

// static int cmd_player_resume(int argc, char **argv)
// {
// 	evs_handler_post_runnable(_handle_cmd_player_resume, NULL);
// 	return 0;
// }

// static int cmd_player_setloglev(int argc, char **argv)
// {
// 	if (argc < 1) {
// 		printf("invalid index %d\n", argc);
// 		return -1;
// 	} else {
// 		int lev = atoi(argv[0]);
// 		if (lev >= 0 && lev <= 5) {
// 			printf("set player log level %d\n", lev);
// 			lisa_log_set_level(lev);
// 		} else {
// 			printf("invalid log level %d\n", lev);
// 		}
// 	}
// 	return 0;
// }

// static int cmd_player_help(int argc, char **argv);
// static const struct listen_cmd_t g_listen_player_cmds[] = {
// 		{"play", cmd_player_play, "player play, ex: listen player play http://***"},
// 		{"stop", cmd_player_stop, "player stop, ex: listen player stop"},
// 		{"pause", cmd_player_pause, "player pause, ex: listen player pause"},
// 		{"resume", cmd_player_resume, "player resume, ex: listen player resume"},
// 		{"setloglev", cmd_player_setloglev, "player setloglev, ex: listen player setloglev [0-5]"},
// 		{"help", cmd_player_help, NULL},
// };

// static int cmd_player_help(int argc, char **argv)
// {
// 	int cmd_len = sizeof(g_listen_player_cmds) / sizeof(g_listen_player_cmds[0]);
// 	for (int i = 0; i < cmd_len; i++) {
// 		if (g_listen_player_cmds[i].help != NULL &&
// 			strcmp(g_listen_player_cmds[i].name, "help") != 0) {
// 			printf("listen player %-16s\t:\t%s\n",
// 					g_listen_player_cmds[i].name,
// 					g_listen_player_cmds[i].help);
// 		}
// 	}
// 	return 0;
// }

// static int cmd_listen_player_exec(int argc, char **argv)
// {
// 	if (argc > 0 && argv[0] != NULL) {
// 		int cmd_len = sizeof(g_listen_player_cmds) / sizeof(g_listen_player_cmds[0]);
// 		for (int i = 0; i < cmd_len; i++) {
// 			if (strcmp(g_listen_player_cmds[i].name, argv[0]) == 0) {
// 				// 去除player
// 				return g_listen_player_cmds[i].exec(argc - 1, argv + 1);
// 			}
// 		}
// 	}

// 	cmd_player_help(0, NULL);
// 	return 0;
// }

// static int cmd_listen_wifi_connect(int argc, char **argv)
// {
// 	if (argc < 2) {
// 		printf("invalid param index %d\n", argc);
// 		return -1;
// 	} else {
// 		char *ssid = argv[0];
// 		char *pwd = argv[1];
// 		if (ssid == NULL || strlen(ssid) == 0 || pwd == NULL || strlen(pwd) == 0) {
// 			printf("wifi param error");
// 			return -1;
// 		}
// 		ls_wifi_connect(ssid, pwd);
// 		return 0;
// 	}
// 	return -1;
// }

// static int cmd_listen_tasks(int argc, char **argv)
// {
// 	uint32_t tasks = uxTaskGetNumberOfTasks();
// 	TaskStatus_t *item = lisa_mem_alloc(tasks * sizeof(TaskStatus_t));
// 	if (item) {
// 		uint32_t total = 0;
// 		tasks = uxTaskGetSystemState(item, tasks, &total);
// 		if (total > 0) {
// 			printf("%s", "\n-------------------------------------------------------------------------------------\n");
// 			printf("%s", "Name                      State  Prio  Stack  MinFree    Tid    Call100US      PCT\n");
// 			printf("%s", "-------------------------------------------------------------------------------------\n");
// 			for (uint32_t i = 0, pct = 0; i < tasks; i++) {
// 				if ((pct = (uint32_t)(100.0f * item[i].ulRunTimeCounter / total))) {
// 					printf("%-25s %-6c %-6u %-6u %-10u %-6u %-12u %5u%%\n"
// 						, item[i].pcTaskName
// 						, "XRBSD"[item[i].eCurrentState]
// 						, item[i].uxCurrentPriority
// 						, uxTaskGetStackSize(item[i].xHandle) * sizeof(StackType_t)
// 						, item[i].usStackHighWaterMark * sizeof(StackType_t)
// 						, item[i].xTaskNumber
// 						, item[i].ulRunTimeCounter, pct);
// 				} else {
// 					printf("%-25s %-6c %-6u %-6u %-10u %-6u %-12u %5s%%\n"
// 						, item[i].pcTaskName
// 						, "XRBSD"[item[i].eCurrentState]
// 						, item[i].uxCurrentPriority
// 						, uxTaskGetStackSize(item[i].xHandle) * sizeof(StackType_t)
// 						, item[i].usStackHighWaterMark * sizeof(StackType_t)
// 						, item[i].xTaskNumber
// 						, item[i].ulRunTimeCounter, "<1");
// 				}
// 			}
// 			printf("%s", "-------------------------------------------------------------------------------------\n\n");
// 		}
// 		lisa_mem_free(item);
// 	}
// 	return 0;
// }

// static int cmd_flash_set_log_lev(int argc, char **argv)
// {
// 	if (argc < 1) {
// 		printf("invalid index %d\n", argc);
// 		return -1;
// 	} else {
// 		int lev = atoi(argv[0]);
// 		printf("set app log level %d\n", lev);
// 		lisa_log_set_level(lev);
// 	}
// 	return 0;
// }

// static int cmd_flash_clear(int argc, char **argv)
// {
// 	if (argc > 0) {
// 		char *key = argv[0];

// 		if (evs_pref_delete(key) == 0) {
// 			printf("flash delete config: %s, success\n", key);
// 		} else {
// 			printf("flash has no config: %s\n", key);
// 		}
// 	} else {
// 		printf("flash clear no arg\n");
// 	}
// 	return 0;
// }

// static int cmd_flash_show(int argc, char **argv)
// {
// 	evs_pref_dump();
// 	return 0;
// }

// static int cmd_flash_set(int argc, char **argv)
// {
// 	if (argc < 3) {
// 		printf("invalid index %d\n", argc);
// 		return -1;
// 	} else {
// 		char *type = argv[0];
// 		char *key = argv[1];
// 		char *value = argv[2];
// 		if(!strcmp(type, "string")) {
// 			if (evs_pref_put_string(key, value) != EVS_OK) {
// 				printf("flash set %s:%s failed\n", key, value);
// 			} else {
// 				printf("flash set %s:%s success\n", key, value);
// 			}
// 		} else if(!strcmp(type, "int")) {
// 			int int_temp = atoi(value);
// 			if (evs_pref_put_int(key, int_temp) != EVS_OK) {
// 				printf("flash set %s:%d failed\n", key, int_temp);
// 			} else {
// 				printf("flash set %s:%d success\n", key, int_temp);
// 			}
// 		} else if(!strcmp(type, "bool")) {
// 			int bool_temp = atoi(value);
// 			if (evs_pref_put_bool(key, bool_temp) != EVS_OK) {
// 				printf("flash set %s:%d failed\n", key, bool_temp);
// 			} else {
// 				printf("flash set %s:%d success\n", key, bool_temp);
// 			}
// 		} else {
// 			printf("invalid type %s\n", type);
//             return -1;
// 		}
// 	}
// 	return 0;
// }

// static int cmd_flash_get(int argc, char **argv)
// {
// 	if (argc < 1) {
// 		printf("invalid index %d\n", argc);
// 		return -1;
// 	} else {
// 		char *type = argv[0];
// 		char *key = argv[1];
// 		if(!strcmp(type, "string")) {
// 			char *str_value = NULL;
// 			if (evs_pref_get_string(key, &str_value) != EVS_OK) {
// 				printf("flash get %s failed\n", key);
// 			} else {
// 				printf("flash get %s:%s success\n", key, str_value);
// 			}
// 			if(NULL != str_value) {
// 				lisa_mem_free(str_value);
// 			}
// 		} else if(!strcmp(type, "int")) {
// 			int int_value = 0;
// 			if (evs_pref_get_int(key, &int_value) != EVS_OK) {
// 				printf("flash get %s failed\n", key);
// 			} else {
// 				printf("flash get %s:%d success\n", key, int_value);
// 			}
// 		} else if(!strcmp(type, "bool")) {
// 			bool bool_value = 0;
// 			if (evs_pref_get_bool(key, &bool_value) != EVS_OK) {
// 				printf("flash get %s failed\n", key);
// 			} else {
// 				printf("flash get %s:%d success\n", key, bool_value);
// 			}
// 		} else if(!strcmp(type, "blob")) {
// 			char *blob_buf = NULL;
// 			int blob_len = 0;
// 			if (evs_pref_get_blob(key, &blob_buf, &blob_len) != EVS_OK) {
// 				printf("flash get %s failed\n", key);
// 			} else {
// 				printf("flash get [%s %d]: ", key, blob_len);
// 				for(int i = 0; i < blob_len; i++) {
// 					printf("%x ", blob_buf[i]);
// 				}
// 				printf("success\n");
// 				lisa_mem_free(blob_buf);
// 			}
// 		} else {
// 			printf("invalid type\n");
//             return -1;
// 		}
// 	}

// 	return 0;
// }

// static int cmd_flash_help(int argc, char **argv);
// static const struct listen_cmd_t g_listen_flash_cmds[] = {
// 	{"clear", cmd_flash_clear, "clear flash kv, ex: listen flash clear ***"},
// 	{"show", cmd_flash_show, "show all flash kv, ex: listen flash show"},
// 	{"set", cmd_flash_set, "set kv to flash (type: int/bool/string/blob), ex: listen flash set int *** ***"},
// 	{"get", cmd_flash_get, "get kv from flash, ex: listen flash get int ***"},
// 	{"setloglev", cmd_flash_set_log_lev, "set app log level (1-5), ex: listen flash setloglev 4"},
// 	{"help", cmd_flash_help, NULL}
// };

// static int cmd_listen_flash_exec(int argc, char **argv)
// {
// 	if (argc > 0 && argv[0] != NULL) {
// 		int cmd_len = sizeof(g_listen_flash_cmds) / sizeof(g_listen_flash_cmds[0]);
// 		for (int i = 0; i < cmd_len; i++) {
// 			if (strcmp(g_listen_flash_cmds[i].name, argv[0]) == 0) {
// 				// 去除flash和name
// 				return g_listen_flash_cmds[i].exec(argc - 1, argv + 1);
// 			}
// 		}
// 	}

// 	cmd_flash_help(0, NULL);
// 	return 0;
// }

// static int cmd_flash_help(int argc, char **argv)
// {
// 	int cmd_len = sizeof(g_listen_flash_cmds) / sizeof(g_listen_flash_cmds[0]);
// 	for (int i = 0; i < cmd_len; i++) {
// 		if (g_listen_flash_cmds[i].help != NULL &&
// 			strcmp(g_listen_flash_cmds[i].name, "help") != 0) {
// 			printf("listen flash %-17s\t:\t%s\n",
// 					g_listen_flash_cmds[i].name,
// 					g_listen_flash_cmds[i].help);
// 		}
// 	}
// 	return 0;
// }

// extern void client_wakeup_test();
// static int cmd_wakeup(int argc, char **argv)
// {
// 	client_wakeup_test();
// 	return CLI_SUCCESS;
// }

// static const struct listen_cmd_t g_listen_cmds[] =
// {
//     {"help", cmd_listen_help, ""},
//     {"wifi_connect", cmd_listen_wifi_connect, "wifi connect, ex: listen wifi_connect ssid pwd"},
//     {"tasks", cmd_listen_tasks, "show task info, ex: listen tasks"},
//     {"player", cmd_listen_player_exec, "player cmd group"},
//     {"flash", cmd_listen_flash_exec, "flash cmd group"},
//     {"wakeup", cmd_wakeup, ""},
// };

// static int cmd_listen_help(int argc, char **argv)
// {
// 	int cmd_len = sizeof(g_listen_cmds) / sizeof(g_listen_cmds[0]);
// 	for (int i = 0; i < cmd_len; i++) {
// 		if (g_listen_cmds[i].help != NULL && strcmp(g_listen_cmds[i].name, "help") != 0) {
// 			printf("listen %-23s\t:\t%s\n", g_listen_cmds[i].name, g_listen_cmds[i].help);
// 		}
// 	}
// 	return CLI_SUCCESS;
// }

// uint32_t listen_cmd_handler(char *command, int len)
// {
//     uint32_t res = CLI_UNKNOWN_CMD;

//     char *argv[LS_CMD_MAX_NUM] = {0};
// 	int argc = _split(command, " ", argv, LS_CMD_MAX_NUM);
// 	if (argc == 1) {
//         res = cmd_listen_help(0, NULL);
// 	} else if ((argc > 1) && (strcmp(argv[0], "listen") == 0)) {
//         if (argv[1] != NULL) {
//             int cmd_len = sizeof(g_listen_cmds) / sizeof(g_listen_cmds[0]);
//             for (int i = 0; i < cmd_len; i++) {
//                 if (strcmp(g_listen_cmds[i].name, argv[1]) == 0) {
// 					// 去除listen和function name
//                     res = g_listen_cmds[i].exec(argc - 2, argv + 2);
//                     break;
//                 }
//             }
//         }
//     }

// 	for(int i = 0; i < argc; i++) lisa_mem_free(*(argv + i));

//     return res;
// }