/**
 ****************************************************************************************
 *
 * @file cli_main.h
 *
 * @brief CLI cmd handle.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */
#ifndef _CLI_MAIN_H_
#define _CLI_MAIN_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "rtos_al.h"
#include "log_print.h"

typedef void (*cli_print_fn_t)(const char *fmt, ...);
extern cli_print_fn_t cli_print_func; // set default
void set_cli_print_func(cli_print_fn_t new_func);

#define CLI_TAG //"CLI:"
#define cli_printn(fmt, ...) \
    do { \
        if (cli_print_func) cli_print_func(fmt"\n", ##__VA_ARGS__); \
    } while (0)
#define CLI_LOG(fmt, ...)     do {cli_printn(CLI_TAG fmt,##__VA_ARGS__);} while(0)
#define CLI_LOGE(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_ERROR)  { cli_printn(CLI_TAG "ERR"fmt,##__VA_ARGS__);}} while(0)
#define CLI_LOGW(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_WARN)   { cli_printn(CLI_TAG "WRN"fmt,##__VA_ARGS__);}} while(0)
#define CLI_LOGI(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_INFO)   { cli_printn(CLI_TAG "INF"fmt,##__VA_ARGS__);}} while(0)
#define CLI_LOGD(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_DEBUG)  { cli_printn(CLI_TAG "DBG"fmt,##__VA_ARGS__);}} while(0)
#define CLI_LOGV(fmt, ...)     do {if (cloglvl >= CLOG_LEVEL_VERBOSE){ cli_printn(CLI_TAG "VBS"fmt,##__VA_ARGS__);}} while(0)

/// CLI command description
struct cli_cmd
{
    /// process function
    int (*exec) (char *params);
    /// name of the command
    char *name;
    /// List of parameters (to report command usage)
    char *params;
};

/// CLI Command result
enum cli_res
{
    CLI_SUCCESS,
    CLI_ERROR,
    CLI_UNKNOWN_CMD,
    CLI_NO_RESP,
    CLI_SHOW_USAGE,
    CLI_INVALID_PARAM,
};


enum cli_err_id
{
    CLI_ERR_UNSPECIF = 10,
    CLI_ERR_NOMEM,
    CLI_ERR_CFGSSID,
    CLI_ERR_CFGAKM,
    CLI_ERR_CFGBSSID,
    CLI_ERR_CFGCHAN,
    CLI_ERR_CFGKEY,
    CLI_ERR_CFGSEC,
    CLI_ERR_FREQ,
    CLI_ERR_DHCPMODE,
    CLI_ERR_IP,
    CLI_ERR_WAKE_DUR,


    CLI_ERR_MAX,
};

enum
{
    /// Priority of the CLI task
    CLI_TASK_PRIORITY = RTOS_TASK_PRIORITY(9),
    /// Priority of the TG send task
    CLI_TG_SEND_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the Ping send task
    CLI_PING_SEND_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the IPERF task
    CLI_IPERF_PRIORITY = RTOS_TASK_PRIORITY(7),
    /// Priority of the DOORBELL task
    CLI_DOORBELL_PRIORITY = RTOS_TASK_PRIORITY(9),
};

/// Stack size of tasks created by the CLI application
enum
{
    /// CLI task stack size
    CLI_CLI_STACK_SIZE = 512,
    /// TG send stack size
    CLI_TG_SEND_STACK_SIZE = 1024,
    /// Ping send stack size
    CLI_PING_SEND_STACK_SIZE = 512,
    /// IPERF task stack size
    CLI_IPERF_STACK_SIZE = 512,
    /// DOORBELL task stack size
    CLI_DOORBELL_STACK_SIZE = 1024,
};

typedef int (*net_cli_sigkill_cb)(rtos_task_handle);
typedef rtos_task_handle (*net_cli_start_task_cb)(void *);
char *utils_next_token(char **params);
size_t utils_get_proper_ssid_psk(char *token, uint8_t *str, uint8_t len);
int utils_parse_mac_addr(char *str, uint8_t *addr);
int utils_cli_parse_ip4(char *str, uint32_t *ip, uint32_t *mask);

uint32_t bt_cmd_handler(char* command, int len);

int32_t cli_shell_process(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t));

#endif /* _CLI_MAIN_H_ */
