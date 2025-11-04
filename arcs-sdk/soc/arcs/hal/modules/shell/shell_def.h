#ifndef _SHELL_DEF_H_
#define _SHELL_DEF_H_
#include <stdint.h>
#include "shell_uart.h"

#define SHELL_CMD_LEN_MAX       128 //1500
#define SHELL_PROMPT_STRING     "ListenAI>"
#define SHELL_ENTER_STRING      "\r\n"

#define SHELL_KEY_DELETE        0x1B5B337E//0x7F000000
#define SHELL_KEY_BACKSPACE     0x08000000 /*'\b'*/
#define SHELL_KEY_CR            0x0D000000 /*'\r'*/
#define SHELL_KEY_LF            0x0A000000 /*'\n'*/
#define SHELL_KEY_TAB           0x09000000 /*'\t'*/
#define SHELL_KEY_LEFT          0x1B5B4400
#define SHELL_KEY_UP            0x1B5B4100
#define SHELL_KEY_RIGHT         0x1B5B4300
#define SHELL_KEY_DOWN          0x1B5B4200
#define SHELL_KEY_INVALID       0xFFFFFFFF
#define SHELL_DEL_DIR_LEFT      0
#define SHELL_DEL_DIR_RIGHT     1

#define SHELL_HISTORY_NUMBER    1

#define SHELL_TASK_PRIORITY     RTOS_TASK_PRIORITY(CONFIG_ARCS_HAL_MODULE_SHELL_TASK_PRIORITY)
#define SHELL_TASK_STACK_SIZE   CONFIG_ARCS_HAL_MODULE_SHELL_TASK_STACK_SIZE

typedef int32_t (*shell_process)(char *command, int32_t len, int32_t (*func)(uint8_t*, int32_t));

#if SHELL_HISTORY_NUMBER > 0
struct shell_history
{
    char item[SHELL_HISTORY_NUMBER][SHELL_CMD_LEN_MAX];
    int32_t  number;
    int32_t record;
    int32_t offset;
};
#endif /** SHELL_HISTORY_MAX_NUMBER > 0 */

struct shell_parser {
    uint32_t length;
    uint32_t cursor;
    char buffer[SHELL_CMD_LEN_MAX];
    uint32_t buffer_size;
    uint32_t key_value;
};

struct shell_env {
    struct shell_parser parser;
#if SHELL_HISTORY_NUMBER > 0
    struct shell_history history;
#endif
    uint32_t (*input)(uint8_t* buf, int32_t len);
    int32_t (*output)(uint8_t* buf, int32_t len);
    shell_process process;
};


int32_t shell_init(shell_process process_fun);


#define shell_malloc     rtos_malloc
#define shell_free       rtos_free

#endif
