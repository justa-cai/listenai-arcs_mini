/*
 * Copyright (c) 2024 ListenAI
 * SPDX-License-Identifier: MIT
 *
 * cAT AT Command Parser - Unsolicited Example
 *
 * This example demonstrates the unsolicited/async response feature of cAT:
 *   - Sending multiple asynchronous responses to a single command
 *   - Using cat_trigger_unsolicited_read() for async notifications
 *   - Variable validators to reject invalid input
 *   - HOLD return states for deferred responses
 *
 * Supported AT commands:
 *   AT+START=<mode>   - Start scanning (0=WiFi, 1=Bluetooth)
 *   AT+SCAN=?         - Test command for scan result format
 *   AT#HELP           - List all commands
 *   AT#QUIT           - Exit the demo
 *
 * The +START command triggers multiple +SCAN unsolicited responses,
 * simulating an async scan operation.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "cat.h"
#include "cat_uart_adapter.h"

#define LOG_TAG "cat_unsolicited"
#include "lisa_log.h"

#ifdef CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

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

/* Variables for scan result */
static int rssi;
static char ssid[16];

/* Helper variable used to exit demo code */
static bool quit_flag;

/* Variables for start command */
static int mode;

/* Main AT command parser object */
static struct cat_object at;

/* ============================================================================
 * Simulated Scan Results
 * ============================================================================ */

struct scan_results {
    int rssi;
    char ssid[16];
};

/* Static const scan results for WiFi (mode 0) and Bluetooth (mode 1) */
static const struct scan_results results[2][3] = {
    /* WiFi results */
    {
        {
            .rssi = -10,
            .ssid = "wifi1",
        },
        {
            .rssi = -50,
            .ssid = "wifi2",
        },
        {
            .rssi = -20,
            .ssid = "wifi3",
        }
    },
    /* Bluetooth results */
    {
        {
            .rssi = -20,
            .ssid = "bluetooth1",
        },
        {
            .rssi = 0,
            .ssid = "",
        },
        {
            .rssi = 0,
            .ssid = "",
        }
    }
};

/* Helper variable for tracking scan progress */
static int scan_index;
static bool scan_completed = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void load_scan_results(int index)
{
    rssi = results[mode][index].rssi;
    strcpy(ssid, results[mode][index].ssid);
}

/* ============================================================================
 * AT Command Variables Definition
 * ============================================================================ */

/* Variables for SCAN command (unsolicited response) */
static struct cat_variable scan_vars[] = {
    {
        .type = CAT_VAR_INT_DEC,
        .data = &rssi,
        .data_size = sizeof(rssi),
        .name = "RSSI",
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = ssid,
        .data_size = sizeof(ssid),
        .name = "SSID",
        .access = CAT_VAR_ACCESS_READ_ONLY,
    }
};

/* Forward declaration of scan read handler */
static cat_return_state scan_read(const struct cat_command *cmd, uint8_t *data,
                                   size_t *data_size, const size_t max_data_size);

/* Unsolicited SCAN command - used for async responses */
static struct cat_command scan_cmd = {
    .name = "+SCAN",
    .description = "Scan result record",
    .read = scan_read,
    .var = scan_vars,
    .var_num = sizeof(scan_vars) / sizeof(scan_vars[0])
};

/* ============================================================================
 * AT Command Handlers
 * ============================================================================ */

/* Unsolicited read callback handler - called for each scan result */
static cat_return_state scan_read(const struct cat_command *cmd, uint8_t *data,
                                   size_t *data_size, const size_t max_data_size)
{
    int max = (mode == 0) ? 3 : 1;

    /* Ignore if scan already completed */
    if (scan_completed) {
        return CAT_RETURN_STATE_OK;
    }

    LOGI("Sending scan result %d: RSSI=%d, SSID=%s", scan_index, rssi, ssid);

    scan_index++;

    /* Load next scan result if not done */
    if (scan_index < max) {
        load_scan_results(scan_index);
        /* Trigger another unsolicited read for the next result */
        cat_trigger_unsolicited_read(&at, &scan_cmd);
        return CAT_RETURN_STATE_DATA_NEXT;
    }

    /* Last result sent, complete with OK and exit HOLD state */
    LOGI("Scan complete, sent %d results", scan_index);
    scan_completed = true;
    
    /* Exit HOLD state to allow next command */
    cat_hold_exit(&at, CAT_STATUS_OK);
    return CAT_RETURN_STATE_DATA_OK;
}

/* Mode variable validator - only accept 0 (WiFi) or 1 (Bluetooth) */
static int mode_write(const struct cat_variable *var, const size_t write_size)
{
    if (*(int *)var->data >= 2) {
        LOGE("Invalid mode: %d (must be 0 or 1)", *(int *)var->data);
        return -1;
    }
    LOGI("Mode set to: %d (%s)", *(int *)var->data,
     (*(int *)var->data == 0) ? "WiFi" : "Bluetooth");
    return 0;
}

/* AT+START=<mode> - Start scanning */
static cat_return_state start_write(const struct cat_command *cmd, const uint8_t *data,
                                     const size_t data_size, const size_t args_num)
{
    LOGI("Starting %s scan...", (mode == 0) ? "WiFi" : "Bluetooth");

    /* Reset scan state */
    scan_index = 0;
    scan_completed = false;

    /* Load first scan result */
    load_scan_results(scan_index);

    /* Trigger the first unsolicited read */
    cat_trigger_unsolicited_read(&at, &scan_cmd);

    /* Return HOLD to defer OK response until scan completes */
    return CAT_RETURN_STATE_HOLD;
}

/* AT#HELP - Print command list */
static int print_cmd_list(const struct cat_command *cmd)
{
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

/* AT#QUIT - Quit command */
static int quit_run(const struct cat_command *cmd)
{
    LOGI("Quit command received");
    quit_flag = true;
    return 0;
}

/* ============================================================================
 * AT Command Variables for START
 * ============================================================================ */

static struct cat_variable start_vars[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &mode,
        .data_size = sizeof(mode),
        .name = "MODE",
        .write = mode_write,
        .access = CAT_VAR_ACCESS_WRITE_ONLY,
    }
};

/* ============================================================================
 * AT Commands Definition
 * ============================================================================ */

static struct cat_command cmds[] = {
    {
        .name = "+START",
        .description = "Start scanning (0=WiFi, 1=Bluetooth)",
        .write = start_write,
        .var = start_vars,
        .var_num = sizeof(start_vars) / sizeof(start_vars[0]),
        .need_all_vars = true
    },
    {
        .name = "+SCAN",
        .description = "Scan result record (unsolicited)",
        .only_test = true,
        .var = scan_vars,
        .var_num = sizeof(scan_vars) / sizeof(scan_vars[0])
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
    .name = "unsolicited",
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
    .buf = buf,
    .buf_size = sizeof(buf),
};

/* Parser object is declared above as 'at' */

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
    LOGI("cAT AT Command Parser - Unsolicited Example");
    LOGI("============================================");

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
 * This example demonstrates unsolicited responses - multiple async responses
 * to a single command.
 *
 * AT+START=0  -> Starts WiFi scan, returns multiple +SCAN results:
 *                +SCAN: -10,"wifi1"
 *                +SCAN: -50,"wifi2"
 *                +SCAN: -20,"wifi3"
 *                OK
 *
 * AT+START=1  -> Starts Bluetooth scan, returns:
 *                +SCAN: -20,"bluetooth1"
 *                OK
 *
 * AT+SCAN=?   -> Shows scan result format (test command)
 *                +SCAN: RSSI(INT),SSID(STRING)
 *                OK
 *
 * AT#HELP     -> Lists all available commands
 *
 * AT#QUIT     -> Exit the demo
 *
 * Key concepts demonstrated:
 * - CAT_RETURN_STATE_HOLD: Defer OK response until async operation completes
 * - CAT_RETURN_STATE_DATA_NEXT: More data follows
 * - CAT_RETURN_STATE_HOLD_EXIT_OK: Complete async operation with OK
 * - cat_trigger_unsolicited_read(): Trigger async response
 *
 * ============================================================================ */
