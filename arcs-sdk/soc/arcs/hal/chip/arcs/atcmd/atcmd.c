/**
 ****************************************************************************************
 *
 * @file atcmd.c
 *
 * @brief
 *
 * Copyright (C) ListenAI  2023-2024
 *
 ****************************************************************************************
 */
#include "atcmd.h"
#include "ls_event.h"
#include "log_print.h"
#include "wifi_api.h"
#include "atcmd_hash.h"
#include "rtos_al.h"

#include "atcmd_tcpip.h"

extern void set_shell_echo(uint8_t enable);

char at_string[AT_STRING_LEN] = {0};

unsigned char  gATLogLevel = AT_LOG_LEVEL_DEBUG;
unsigned int   gATLogFlag = AT_LOG_FLAG_LWIP;

const char *atcmd_res_str[ATCMD_RES_MAX] =
{
    "OK",
    "ERROR",
    "UNKNOWN"
};

#if BT_WIFI_COEX
extern void hci_event_notify_reg(void *notify);
#endif
int atcmd_help(int type, char *params);

void atcmd_handler(char* command, int len)
{
    int ret;
    char *cmd_start;
    char *param;
    const atcmd_entry_t *cmd = NULL;
    int8_t atcmd_type = ATCMD_EXEC;

    cmd_start = strstr(command, "AT");
    if (!cmd_start)
    {
        atcmd_rspinfor("%s", atcmd_res_str[ATCMD_UNKNOWN]);
        return;
    }
    #if BT_WIFI_COEX
    hci_event_notify_reg(ls_event_post);
    #endif
    param = cmd_start + 2;
    if (*param == '\0')
    {
        atcmd_rspinfor("%s", atcmd_res_str[ATCMD_OK]);
    }
    else if (*param == 'E')
    {
        param += 1;
        if (*param == '0' || *param == '1')
        {
            ret = atcmd_echo(atcmd_type, param);
            atcmd_rspinfor("%s", atcmd_res_str[ret]);
        }
        else
        {
            atcmd_rspinfor("%s", atcmd_res_str[ATCMD_UNKNOWN]);
        }
    }
    else if (*param == '?')
    {
        ret = atcmd_help(atcmd_type, param);
        atcmd_rspinfor("%s", atcmd_res_str[ret]);
    }
    else if (*param == '+')
    {
        param = strchr(cmd_start, '=');
        if (param)
        {
            *param++ = '\0';
            if (!strcmp(param, "?"))
            {
                atcmd_type = ATCMD_PARAM;
                //CLOGI("%s %d\n",__func__, __LINE__);
            }
            else
            {
                atcmd_type = ATCMD_EXEC;
                //CLOGI("%s %d\n",__func__, __LINE__);
            }
        }
        else
        {
            param = strchr(cmd_start, '?');
            if (param)
            {
                *param = '\0';
                atcmd_type = ATCMD_QUERY;
                //CLOGI("%s %d\n",__func__, __LINE__);
            }
            else
            {
                atcmd_type = ATCMD_EXEC;
                //CLOGI("%s %d\n",__func__, __LINE__);
            }
        }

        cmd = (const atcmd_entry_t*)atcmd_item_action(cmd_start);

        if ((cmd != NULL) && (cmd->func))
        {
            ret = cmd->func(atcmd_type, param);
            atcmd_rspinfor("%s", atcmd_res_str[ret]);
        }
        else
        {
            atcmd_rspinfor("%s", atcmd_res_str[ATCMD_UNKNOWN]);
        }
    }
    else
    {
        atcmd_rspinfor("%s", atcmd_res_str[ATCMD_UNKNOWN]);
    }
}

int32_t atcmd_shell_process(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t))
{
    uint32_t res = 0;
    if (!memcmp(command, "AT", 2)) {
        atcmd_handler(command, len);
    } else if (!memcmp(command, "soc", 3)) {
        atcmd_rspinfor("command is %s\n",command);
        // tcpip_cmd_handler(command, len);
    } else if (!memcmp(command, "wifi", 4)) {
        res = wifi_cmd_handler(command, len);
    } else {
        res = cli_cmd_handler(command, len);
    }

    return 0;
}

bool atcmd_char_is_escape_char(char *ptr)
{
    int num = 0;

    while (*(ptr - 1) == '\\')
    {
        num++;
        ptr -= 1;
    }

    if (num % 2 != 0)
    {
        return true;
    }

    return false;
}

/**
 ****************************************************************************************
 * @brief Extract token from parameter list
 *
 * Extract the first parameter of the string. Parameters are separatd with ',' unless
 * it starts with '"' in which case it extract the token until '"' is reached.
 * '"' is then removed from the token.
 *
 * @param[in, out] params Pointer to parameters string to parse. Updated with remaining
 *                        parameters to parse.
 * @return pointer on first parameter
 ****************************************************************************************
 */
char *atcmd_next_token(char **params)
{
    char *str;
    char *ptr = *params;
    char *next;
    char sep  = ',';
    uint8_t sep_is_default = 1;

    if (!ptr)
    {
        return NULL;
    }

    // ingnore beginning ' '
    while(*ptr == ' ')
    {
        ptr++;
    }
    str = ptr;

    if (*ptr == '\0')
    {
        return NULL;
    }

    // if token is started by '"'
    if (ptr[0] == '"')
    {
        sep = ptr[0];
        sep_is_default = 0;
        ptr++;
        str = ptr;
    }
    else if (ptr[0] == ',')
    {
        //current token is NULL
        str = ptr;
        ptr = NULL;
    }

    while (str && (next = strchr(str, sep)))
    {
        // check if is escape character
        if (!atcmd_char_is_escape_char(next))
        {
            *next++ = '\0';
            while (*next == ' ')
            {
                next++;
            }

            if (*next == '\0')
            {
                next = NULL;
            }
            else if (!sep_is_default) // find next original seperate char ","
            {
                if (*next != ',')
                {
                    next = NULL;
                    CLOGI("unexpected atcmd input");
                }
                else
                {
                    // ignore the ','
                    next++;

                    while (*next == ' ')
                    {
                        next++;
                    }
                    if (*next == '\0')
                    {
                        next = NULL;
                    }
                }
            }

            break;
        }
        else
        {
            if (*next++ == '\0')
                 next = NULL;
            str = next;
        }
    }
    *params = next;

    return ptr;
}

int atcmd_echo(int type, char *params)
{
    if (!strcmp(params, "0"))
    {
        //disable echo
        set_shell_echo(0);
        return ATCMD_OK;
    }
    else if (!strcmp(params, "1"))
    {
        //enable echo
        set_shell_echo(1);
        return ATCMD_OK;
    }
    else
    {
        return ATCMD_UNKNOWN;
    }
}

static void at_rtos_info(void)
{
    int total, used, free, min_free_size;
    uint8_t *task_info;

    rtos_heap_info(&total, &free, &min_free_size);
    atcmd_rspinfor("RTOS HEAP:\n"
			   "=================================\n"
			   "TotalSize   FreeSize    MinFreeSz\n"
			   "%-9d   %-9d   %-9d\n"
			   "=================================\n",
				total,
				free,
				min_free_size);
    uint32_t tasks = uxTaskGetNumberOfTasks();
	TaskStatus_t *item = rtos_malloc(tasks * sizeof(TaskStatus_t));
	if (item) {
		uint32_t total = 0;
		tasks = uxTaskGetSystemState(item, tasks, &total);
		if (total > 0) {
			atcmd_rspinfor("%s", "\n-------------------------------------------------------------------------------------");
			atcmd_rspinfor("%s", "Name                      State  Prio  Stack  MinFree    Tid    Call             PCT");
			atcmd_rspinfor("%s", "-------------------------------------------------------------------------------------");
			for (uint32_t i = 0, pct = 0; i < tasks; i++) {
				if ((pct = (uint32_t)(100.0f * item[i].ulRunTimeCounter / total))) {
					atcmd_rspinfor("%-25s %-6c %-6u %-6u %-10u %-6u %-12u %5u%%"
						, item[i].pcTaskName
						, "XRBSD"[item[i].eCurrentState]
						, item[i].uxCurrentPriority
						, (item[i].pxEndOfStack - item[i].pxStackBase + 2) * sizeof(StackType_t)//uxTaskGetStackSize(item[i].xHandle) * sizeof(StackType_t)
						, item[i].usStackHighWaterMark * sizeof(StackType_t)
						, item[i].xTaskNumber
						, item[i].ulRunTimeCounter, pct);
				} else {
					atcmd_rspinfor("%-25s %-6c %-6u %-6u %-10u %-6u %-12u %5s%%"
						, item[i].pcTaskName
						, "XRBSD"[item[i].eCurrentState]
						, item[i].uxCurrentPriority
						, (item[i].pxEndOfStack - item[i].pxStackBase + 2) * sizeof(StackType_t)//uxTaskGetStackSize(item[i].xHandle) * sizeof(StackType_t)
						, item[i].usStackHighWaterMark * sizeof(StackType_t)
						, item[i].xTaskNumber
						, item[i].ulRunTimeCounter, "<1");
				}
			}
			atcmd_rspinfor("%s", "-------------------------------------------------------------------------------------\n");
		}
		rtos_free(item);
	}
}

int atcmd_rtos_info(int type, char *params)
{
    ls_err_t ret;
    uint8_t enable = 0;

    if (type == ATCMD_PARAM)
    {
        return ATCMD_UNKNOWN;
    }
    else if (type == ATCMD_EXEC)
    {
        return ATCMD_UNKNOWN;
    }
    else //ATCMD_QUERY
    {
        at_rtos_info();
        return ATCMD_OK;
    }
}

const atcmd_item_t atcmd_local_table[] =
{
    // common
    {atcmd_help, "AT?", "get all atcmd help"},
    {atcmd_echo, "ATE", "ATE1 : enable atcmd echo, ATE0 : disable atcmd echo"},
    {atcmd_rtos_info, "AT+RTOSINF", "get rtos info"},
};

void atcmd_local_register(void)
{
    atcmd_entry_add_table(atcmd_local_table, sizeof(atcmd_local_table)/sizeof(atcmd_item_t));
}

void atcmd_local_help(void)
{
    int i;
    int item_len;
    item_len = sizeof(atcmd_local_table)/sizeof(atcmd_local_table[0]);
    for (i = 0; i < item_len; i++)
        CLOGI("%s: %s\n", atcmd_local_table[i].atcmd_entry.name, atcmd_local_table[i].atcmd_entry.help);
}

int atcmd_help(int type, char *params)
{
#if defined(CLI_TYPE_WF)
    atcmd_wifi_help();
#endif
    atcmd_local_help();
    atcmd_iperf_help();
    atcmd_lwip_help();
#if BT_WIFI_COEX
    atcmd_bt_help();
#endif
    return ATCMD_OK;
}

void atcmd_init(void)
{
    atcmd_hash_init();
#if defined(CLI_TYPE_WF)
    atcmd_wifi_register();
#endif
    atcmd_local_register();
    atcmd_iperf_register();
    atcmd_lwip_register();
#if BT_WIFI_COEX
    atcmd_bt_register();
#endif
}

