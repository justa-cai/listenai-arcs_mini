/*
 * Copyright (c) 2024 ListenAI
 * SPDX-License-Identifier: MIT
 *
 * cAT AT Command Parser - Demo Example
 *
 * This example demonstrates advanced cAT features including:
 *   - Variable write callbacks for real-time variable update notifications
 *   - Multiple variable types (INT_DEC, UINT_DEC, NUM_HEX, BUF_STRING, BUF_HEX)
 *   - Command write handlers with variable access
 *   - Command read handlers
 *
 * Supported AT commands:
 *   AT+GO=<x>,<y>,<msg>   - Execute with coordinates and message
 *   AT+SET=<speed>,<addr>,<buf> - Set speed, hex address, and hex buffer
 *   AT+SET?               - Query SET variables
 *   AT#TEST               - Run test command
 *   AT#HELP               - List all commands
 *   AT#QUIT               - Exit the demo
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "cat.h"
#include "cat_uart_adapter.h"

#ifdef CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#define LOG_TAG "cat_demo"
#include "lisa_log.h"

#if defined(CONFIG_BOARD_ARCS_MINI) || defined(CONFIG_BOARD_ARCS_EVB)
#include "IOMuxManager.h"

#ifdef CONFIG_BOARD_ARCS_MINI
#include "pinmux.h"
#define UART1_TX_PAD     CSK_IOMUX_PAD_A
#define UART1_TX_PIN     8
#define UART1_RX_PAD     CSK_IOMUX_PAD_A
#define UART1_RX_PIN     9
#else
#define UART1_TX_PAD     CSK_IOMUX_PAD_B
#define UART1_TX_PIN     2
#define UART1_RX_PAD     CSK_IOMUX_PAD_B
#define UART1_RX_PIN     3
#endif
#define UART1_FUNC       CSK_IOMUX_FUNC_ALTER3

void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(UART1_TX_PAD, UART1_TX_PIN, UART1_FUNC);
    IOMuxManager_PinConfigure(UART1_RX_PAD, UART1_RX_PIN, UART1_FUNC);
}
#endif

/* ============================================================================
 * Variables for AT commands
 * ============================================================================ */

static int32_t speed;
static uint16_t adr;
static uint8_t x;
static uint8_t y;
static uint8_t bytes_buf[4];
static char msg[16];
static bool quit_flag;

/* ============================================================================
 * Variable Write Callbacks
 * ============================================================================ */

static int x_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("x variable updated internally to: %u", x);
    return 0;
}

static int y_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("y variable updated internally to: %u", y);
    return 0;
}

static int msg_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("msg variable updated %zu bytes internally to: <%s>", write_size, msg);
    return 0;
}

static int speed_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("speed variable updated internally to: %d", (int)speed);
    return 0;
}

static int adr_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("adr variable updated internally to: 0x%04X", adr);
    return 0;
}

static int bytesbuf_write(const struct cat_variable *var, size_t write_size)
{
    LOGI("bytes_buf variable updated %zu bytes internally to: %02X%02X%02X%02X",
         write_size, bytes_buf[0], bytes_buf[1], bytes_buf[2], bytes_buf[3]);

    return 0;
}

/* ============================================================================
 * AT Command Handlers
 * ============================================================================ */

/* AT+GO=<x>,<y>,<msg> - Execute GO command */
static cat_return_state go_write(const struct cat_command *cmd, const uint8_t *data,
                                  const size_t data_size, const size_t args_num)
{
    LOGI("<%s>: x=%d y=%d msg=%s @ speed=%d",
         cmd->name,
         *(uint8_t *)(cmd->var[0].data),
         *(uint8_t *)(cmd->var[1].data),
         msg,
         (int)speed);

    LOGI("<bytes>: %02X%02X%02X%02X",
         bytes_buf[0], bytes_buf[1], bytes_buf[2], bytes_buf[3]);

    return CAT_RETURN_STATE_OK;
}

/* AT+SET=<speed>,<addr>,<buf> - Set command write handler */
static cat_return_state set_write(const struct cat_command *cmd, const uint8_t *data,
                                   const size_t data_size, const size_t args_num)
{
    LOGI("<%s>: SET SPEED TO = %d", cmd->name, (int)speed);
    return CAT_RETURN_STATE_OK;
}

/* AT+SET? - Set command read handler */
static cat_return_state set_read(const struct cat_command *cmd, uint8_t *data,
                                  size_t *data_size, const size_t max_data_size)
{
    return CAT_RETURN_STATE_DATA_OK;
}

/* AT#TEST - Run test command */
static int test_run(const struct cat_command *cmd)
{
    LOGI("TEST: <%s>", cmd->name);
    return 0;
}

/* AT#QUIT - Quit command */
static int quit_run(const struct cat_command *cmd)
{
    LOGI("QUIT: <%s>", cmd->name);
    quit_flag = true;
    return 0;
}

/* AT#HELP - Print command list */
static int print_cmd_list(const struct cat_command *cmd)
{
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

/* ============================================================================
 * AT Command Variables Definition
 * ============================================================================ */

/* Variables for GO command */
static struct cat_variable go_vars[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &x,
        .data_size = sizeof(x),
        .write = x_write,
        .name = "x",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &y,
        .data_size = sizeof(y),
        .write = y_write,
        .name = "y",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = msg,
        .data_size = sizeof(msg),
        .write = msg_write,
        .name = "msg",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    }
};

/* Variables for SET command */
static struct cat_variable set_vars[] = {
    {
        .type = CAT_VAR_INT_DEC,
        .data = &speed,
        .data_size = sizeof(speed),
        .write = speed_write,
        .name = "speed",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_NUM_HEX,
        .data = &adr,
        .data_size = sizeof(adr),
        .write = adr_write,
        .name = "address",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_HEX,
        .data = &bytes_buf,
        .data_size = sizeof(bytes_buf),
        .write = bytesbuf_write,
        .name = "buffer",
        .access = CAT_VAR_ACCESS_READ_WRITE,
    }
};

/* ============================================================================
 * AT Commands Definition
 * ============================================================================ */

static struct cat_command cmds[] = {
    {
        .name = "+GO",
        .description = "Go to position (x, y) with message",
        .write = go_write,
        .var = go_vars,
        .var_num = sizeof(go_vars) / sizeof(go_vars[0]),
        .need_all_vars = true
    },
    {
        .name = "+SET",
        .description = "Set speed, hex address and hex buffer",
        .write = set_write,
        .read = set_read,
        .var = set_vars,
        .var_num = sizeof(set_vars) / sizeof(set_vars[0]),
    },
    {
        .name = "#TEST",
        .description = "Run test",
        .run = test_run
    },
    {
        .name = "#HELP",
        .description = "Print command list",
        .run = print_cmd_list,
    },
    {
        .name = "#QUIT",
        .description = "Quit the demo",
        .run = quit_run
    },
};

/* Command group */
static struct cat_command_group cmd_group = {
    .name = "demo",
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};

static struct cat_command_group *cmd_desc[] = {
    &cmd_group
};

/* ============================================================================
 * cAT Parser Configuration
 * ============================================================================ */

/* Working buffer */
static char buf[128];

/* Parser descriptor */
static struct cat_descriptor desc = {
    .cmd_group = cmd_desc,
    .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),
    .buf = (uint8_t *)buf,
    .buf_size = sizeof(buf)
};

/* Parser object */
static struct cat_object at;

/* ============================================================================
 * Main Function
 * ============================================================================ */

static void print_supported_commands(void)
{
    size_t i, j;
    
    LOGI("Supported AT commands:");
    for (i = 0; i < desc.cmd_group_num; i++) {
        for (j = 0; j < desc.cmd_group[i]->cmd_num; j++) {
            LOGI("  %s - %s",
                 desc.cmd_group[i]->cmd[j].name,
                 desc.cmd_group[i]->cmd[j].description);
        }
    }
}

int main(void)
{
    LOGI("cAT AT Command Parser - Demo Example");
    LOGI("=====================================");

    /* Initialize with UART adapter */
    cat_uart_config_t uart_config = {
        .uart_device_name = "uart1",
        .baudrate = 115200,
    };

    cat_uart_adapter_t *adapter = cat_uart_adapter_init(&uart_config);
    if (!adapter) {
        LOGE("Failed to initialize UART adapter");
        return -1;
    }

    LOGI("AT command parser ready on %s", uart_config.uart_device_name);
    print_supported_commands();

    /* Start background processing tasks (also initializes cAT parser) */
    cat_uart_adapter_start_service(adapter, &at, &desc);

    while (!quit_flag) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    LOGI("Bye!");
    return 0;
}

/* ============================================================================
 * Example Usage:
 *
 * Send these AT commands to test:
 *
 * AT+GO=10,20,"test msg"  -> Sets x=10, y=20, msg="test msg", returns OK
 * AT+SET=100,0x1234,AABBCCDD -> Sets speed=100, adr=0x1234, buffer=AABBCCDD
 * AT+SET?                 -> Query SET variables
 * AT#TEST                 -> Runs test command
 * AT#HELP                 -> Lists all available commands
 * AT#QUIT                 -> Exit the demo
 *
 * Variable write callbacks will print messages when variables are updated.
 *
 * ============================================================================ */
