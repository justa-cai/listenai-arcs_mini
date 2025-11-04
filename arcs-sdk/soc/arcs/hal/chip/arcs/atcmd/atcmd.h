// Copyright 2024-2025 ListenAI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _ATCMD_H_
#define _ATCMD_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "dlist.h"

#define AT_STRING_LEN      (255)

extern char at_string[AT_STRING_LEN];
extern void shell_output_fmt_string(char *str);
extern void shell_output_data(char *data, uint32_t len);

#define AT_PRINTF(fmt, ...)  do{\
            snprintf(at_string, AT_STRING_LEN, fmt, ##__VA_ARGS__); \
            shell_output_fmt_string(at_string);\
        }while(0)

/*
@brief :log uart printf
*/
#define AT_BIT(n)           (1<<n)

#define AT_LOG_FLAG_COMMON      AT_BIT(0)
#define AT_LOG_FLAG_SYS         AT_BIT(1)
#define AT_LOG_FLAG_WIFI        AT_BIT(2)
#define AT_LOG_FLAG_LWIP        AT_BIT(3)

enum{
	AT_LOG_LEVEL_OFF = 0,
	AT_LOG_LEVEL_ALWAYS,
	AT_LOG_LEVEL_ERROR,
	AT_LOG_LEVEL_WARNING,
	AT_LOG_LEVEL_DEBUG
};

extern unsigned char  gATLogLevel;
extern unsigned int   gATLogFlag;

#define AT_PRINTK(...)			    \
		do{							\
			printf(__VA_ARGS__); 	\
			printf("\r\n");			\
		}while(0)
#define _AT_PRINTK(...)	printf(__VA_ARGS__)
#define AT_DBG_MSG(flag, level, ...)					            \
		do{														    \
			if(((flag) & gATLogFlag) && (level <= gATLogLevel)){	\
				AT_PRINTK(__VA_ARGS__);							    \
			}													    \
		}while(0)
#define _AT_DBG_MSG(flag, level, ...)					            \
		do{														    \
			if(((flag) & gATLogFlag) && (level <= gATLogLevel)){	\
				_AT_PRINTK(__VA_ARGS__);						    \
			}													    \
		}while(0)

#define atcmd_rspinfor(Fmt, ...) AT_PRINTF(Fmt "\r\n", ## __VA_ARGS__)
#define atcmd_rspdata(Fmt, ...)  AT_PRINTF("+" Fmt "\r\n", ## __VA_ARGS__)

#define atcmd_print_data(data, size)            \
        do {                                    \
            shell_output_data(data, size);      \
        } while(0)

typedef enum atcmd_dbg_err_id
{
    ATCMD_ERR_UNSPECIF = 1,
    ATCMD_ERR_QUERY_NO_RESULT,
    ATCMD_ERR_PARAM_INVALID,
    ATCMD_ERR_TIMEOUT,
    ATCMD_ERR_NO_MEM,
    // WiFi
    ATCMD_ERR_CFG_SSID = 11,
    ATCMD_ERR_CFG_KEY,
    ATCMD_ERR_CFG_BSSID,
    ATCMD_ERR_CFG_COUNTRY,
    ATCMD_ERR_CFG_SNIFFER,
}atcmd_dbg_err_id_e;

typedef enum atcmd_res
{
    ATCMD_OK = 0,
    ATCMD_ERROR,
    ATCMD_UNKNOWN,
    ATCMD_RES_MAX,
} atcmd_res_e;

typedef enum atcmd_type
{
    ATCMD_EXEC,
    ATCMD_QUERY,
    ATCMD_PARAM,
    ATCMD_TYPE_MAX,
} atcmd_type_e;

typedef struct atcmd_entry
{
    // process function
    int (*func) (int type, char *params);
    // name of the command
    char *name;
    // command usage description
    char *help;
} atcmd_entry_t;

typedef struct _atcmd_item_ {
    atcmd_entry_t atcmd_entry;
    struct dlist_head node;
} atcmd_item_t;

extern const char *atcmd_res_str[ATCMD_RES_MAX];

int32_t atcmd_shell_process(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t));

char *atcmd_next_token(char **params);

extern void atcmd_wifi_register(void);
extern void atcmd_wifi_help(void);
extern void atcmd_iperf_register(void);
extern void atcmd_iperf_help(void);
extern void atcmd_lwip_register(void);
extern void atcmd_lwip_help(void);
extern void atcmd_bt_register(void);
extern void atcmd_bt_help(void);
void atcmd_init(void);


#endif //_ATCMD_H_
