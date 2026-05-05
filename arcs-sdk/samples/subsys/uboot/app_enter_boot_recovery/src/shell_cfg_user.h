/**
 * @file shell_cfg_user.h
 * @brief Sample-local letter-shell overlay for BOOT_ADB recovery.
 */

#ifndef __SHELL_CFG_USER_H__
#define __SHELL_CFG_USER_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#define SHELL_DEFAULT_USER "boot"
#define SHELL_SHOW_INFO 0
#define SHELL_TASK_WHILE 0
#define SHELL_CLS_WHEN_LOGIN 0
#define SHELL_HELP_LIST_USER 0
#define SHELL_HELP_LIST_VAR 0
#define SHELL_HELP_LIST_KEY 0

#define SHELL_USING_COMPANION 0
#define SHELL_SUPPORT_END_LINE 1
#define SHELL_ENTER_LF 0
#define SHELL_ENTER_CR 1
#define SHELL_ENTER_CRLF 0
#define SHELL_SCAN_BUFFER 128
#define SHELL_GET_TICK() xTaskGetTickCount()
#define SHELL_USING_LOCK 0
#define SHELL_MALLOC(size) 0
#define SHELL_FREE(obj) 0

#endif
