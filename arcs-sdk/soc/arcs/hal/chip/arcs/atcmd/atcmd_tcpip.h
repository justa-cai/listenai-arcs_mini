/**
 ****************************************************************************************
 *
 * @file at_tcpip.h
 *
 * @brief CLI cmd for tcpip.
 *
 * Copyright (C) ListenAI 2024 ~ 2025
 *
 ****************************************************************************************
 */
#ifndef _CLI_TCPIP_H_
#define _CLI_TCPIP_H_

#include "atcmd.h"
#include "atcmd_soc.h"

#define SSC_IPV6_ADDR_STR_MAX_LEN           46

#define SSC_MAX_SOC_ALLOWED                 16

#if  0
#define ssc_tcpip_trace         printf
#else
#define ssc_tcpip_trace(...)
#endif

#define MAX_LINE_N  255
#define CMD_BUFFER_LEN  MAX_LINE_N+1
#define CMD_MAX_ARG 32

// string operation functions
#define ssc_os_strlen(x)        strlen((const char *)x)
#define ssc_os_strcpy           strcpy
#define ssc_os_strcat           strcat
#define ssc_os_strcmp           strcmp
#define ssc_os_strchr           strchr
#define ssc_os_strncmp          strncmp
#define ssc_os_strncpy          strncpy

// memory operation functions
#define ssc_os_malloc           malloc
#define ssc_os_zalloc(size)     calloc(size, 1)
#define ssc_os_free             free
#define ssc_os_memcpy           memcpy
#define ssc_os_memset           memset
#define ssc_os_memcmp           memcmp

// print functions
#define ssc_os_printf           printf
#define ssc_os_sprintf          sprintf
#define ssc_os_snprintf         snprintf
// #define ssc_print               CLI_LOGD

// mutex
#define ssc_mutex_t             SemaphoreHandle_t
#define ssc_mutex_create        xSemaphoreCreateMutex
#define ssc_mutex_delete        vSemaphoreDelete
#define ssc_mutex_take          xSemaphoreTake
#define ssc_mutex_give          xSemaphoreGive

#define ssc_print               AT_PRINTF


/*
 * INCLUDE FILES
 ****************************************************************************************
 */
void tcpip_cmd_handler(char* command, int len);

/**
 * @}
 */
#endif /* _CLI_WIFI_H_ */
