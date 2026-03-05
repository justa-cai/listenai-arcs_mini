/*
 * Copyright (c) 2024 ListenAI
 * SPDX-License-Identifier: MIT
 *
 * cAT AT Command Parser - Basic Example
 *
 * This example demonstrates how to use the cAT library for AT command parsing.
 * It creates a simple AT command interface with the following commands:
 *
 *   AT+VERSION?     - Query firmware version
 *   AT+LED=<state>  - Set LED state (0=off, 1=on)
 *   AT+LED?         - Query LED state
 *   AT+GPIO=<pin>,<value> - Set GPIO pin value
 *   AT+GPIO?<pin>   - Query GPIO pin value
 *   AT+RESET        - Reset the device
 *   AT+HELP         - List all available commands
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cat.h"
#include "cat_uart_adapter.h"

#ifdef CONFIG_MODULE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#define LOG_TAG "cat_example"
#include "lisa_log.h"

#ifdef CONFIG_BOARD_ARCS_EVB
#include "IOMuxManager.h"

#define UART1_TX_PAD     CSK_IOMUX_PAD_B
#define UART1_TX_PIN    2
#define UART1_RX_PAD     CSK_IOMUX_PAD_B
#define UART1_RX_PIN    3
#define UART1_FUNC      CSK_IOMUX_FUNC_ALTER3

void lisa_uart1_pinmux()
{
    IOMuxManager_PinConfigure(UART1_TX_PAD, UART1_TX_PIN, UART1_FUNC);
    IOMuxManager_PinConfigure(UART1_RX_PAD, UART1_RX_PIN, UART1_FUNC);
}
#endif

/* ============================================================================
 * Variables for AT commands
 * ============================================================================ */

/* LED state variable */
static int32_t led_state = 0;

/* GPIO variables */
static int32_t gpio_pin = 0;
static int32_t gpio_value = 0;

/* Version string */
static char version_str[32] = "1.0.0";

/* ============================================================================
 * AT Command Handlers
 * ============================================================================ */

/* AT+VERSION? - Read version */
static cat_return_state cmd_version_read(const struct cat_command *cmd,
                                          uint8_t *data, size_t *data_size,
                                          const size_t max_data_size)
{
    (void)cmd;
    *data_size = snprintf((char *)data, max_data_size, "%s", version_str);
    return CAT_RETURN_STATE_DATA_OK;
}

/* AT+LED=<state> - Write LED state */
static cat_return_state cmd_led_write(const struct cat_command *cmd,
                                       const uint8_t *data, const size_t data_size,
                                       const size_t args_num)
{
    (void)cmd;
    (void)data;
    (void)data_size;

    if (args_num < 1) {
        return CAT_RETURN_STATE_ERROR;
    }

    /* led_state is automatically updated by cAT based on variable definition */
    LOGI("LED state set to: %d", (int)led_state);

    /* Validate LED state */
    if (led_state < 0 || led_state > 1) {
        LOGE("Invalid LED state: %d", (int)led_state);
        return CAT_RETURN_STATE_ERROR;
    }

    /* Here you would actually control the LED hardware */
    /* lisa_gpio_write_pin(gpio_dev, LED_PIN, led_state); */

    return CAT_RETURN_STATE_OK;
}

/* AT+LED? - Read LED state */
static cat_return_state cmd_led_read(const struct cat_command *cmd,
                                      uint8_t *data, size_t *data_size,
                                      const size_t max_data_size)
{
    (void)cmd;
    /* The variable will be automatically formatted by cAT */
    (void)data;
    (void)data_size;
    (void)max_data_size;
    return CAT_RETURN_STATE_DATA_OK;
}

/* AT+GPIO=<pin>,<value> - Write GPIO */
static cat_return_state cmd_gpio_write(const struct cat_command *cmd,
                                        const uint8_t *data, const size_t data_size,
                                        const size_t args_num)
{
    (void)cmd;
    (void)data;
    (void)data_size;

    if (args_num < 2) {
        return CAT_RETURN_STATE_ERROR;
    }

    LOGI("GPIO %d set to: %d", (int)gpio_pin, (int)gpio_value);

    /* Validate GPIO pin and value */
    if (gpio_pin < 0 || gpio_pin > 31) {
        LOGE("Invalid GPIO pin: %d", (int)gpio_pin);
        return CAT_RETURN_STATE_ERROR;
    }

    if (gpio_value < 0 || gpio_value > 1) {
        LOGE("Invalid GPIO value: %d", (int)gpio_value);
        return CAT_RETURN_STATE_ERROR;
    }

    /* Here you would actually control the GPIO hardware */
    /* lisa_gpio_write_pin(gpio_dev, gpio_pin, gpio_value); */

    return CAT_RETURN_STATE_OK;
}

/* AT+GPIO? - Read GPIO */
static cat_return_state cmd_gpio_read(const struct cat_command *cmd,
                                       uint8_t *data, size_t *data_size,
                                       const size_t max_data_size)
{
    (void)cmd;
    (void)data;
    (void)data_size;
    (void)max_data_size;

    /* Here you would actually read from GPIO hardware */
    /* gpio_value = lisa_gpio_read_pin(gpio_dev, gpio_pin); */

    return CAT_RETURN_STATE_DATA_OK;
}

/* AT+RESET - Reset device */
static cat_return_state cmd_reset_run(const struct cat_command *cmd)
{
    (void)cmd;
    LOGI("Device reset requested");

    /* Here you would trigger a device reset */
    /* NVIC_SystemReset(); */

    return CAT_RETURN_STATE_OK;
}

/* AT+HELP - Print all commands */
static cat_return_state cmd_help_run(const struct cat_command *cmd)
{
    (void)cmd;
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

/* ============================================================================
 * AT Command Variables Definition
 * ============================================================================ */

/* Variables for LED command */
static struct cat_variable led_vars[] = {
    {
        .name = "state",
        .type = CAT_VAR_INT_DEC,
        .data = &led_state,
        .data_size = sizeof(led_state),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
};

/* Variables for GPIO command */
static struct cat_variable gpio_vars[] = {
    {
        .name = "pin",
        .type = CAT_VAR_INT_DEC,
        .data = &gpio_pin,
        .data_size = sizeof(gpio_pin),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .name = "value",
        .type = CAT_VAR_INT_DEC,
        .data = &gpio_value,
        .data_size = sizeof(gpio_value),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
};

/* ============================================================================
 * AT Commands Definition
 * ============================================================================ */

static struct cat_command commands[] = {
    {
        .name = "+VERSION",
        .description = "Firmware version",
        .read = cmd_version_read,
    },
    {
        .name = "+LED",
        .description = "LED control (0=off, 1=on)",
        .write = cmd_led_write,
        .read = cmd_led_read,
        .var = led_vars,
        .var_num = sizeof(led_vars) / sizeof(led_vars[0]),
    },
    {
        .name = "+GPIO",
        .description = "GPIO control (pin, value)",
        .write = cmd_gpio_write,
        .read = cmd_gpio_read,
        .var = gpio_vars,
        .var_num = sizeof(gpio_vars) / sizeof(gpio_vars[0]),
    },
    {
        .name = "+RESET",
        .description = "Reset device",
        .run = cmd_reset_run,
    },
    {
        .name = "+HELP",
        .description = "List all commands",
        .run = cmd_help_run,
    },
};

/* Command group */
static struct cat_command_group cmd_group = {
    .name = "basic",
    .cmd = commands,
    .cmd_num = sizeof(commands) / sizeof(commands[0]),
};

static struct cat_command_group *cmd_groups[] = {
    &cmd_group,
};

/* ============================================================================
 * cAT Parser Configuration
 * ============================================================================ */

/* Working buffer */
static uint8_t cat_buf[256];

/* Parser descriptor */
static struct cat_descriptor cat_desc = {
    .cmd_group = cmd_groups,
    .cmd_group_num = sizeof(cmd_groups) / sizeof(cmd_groups[0]),
    .buf = cat_buf,
    .buf_size = sizeof(cat_buf),
};

/* Parser object */
static struct cat_object cat;

/* ============================================================================
 * Main Function
 * ============================================================================ */

static void print_supported_commands(void)
{
    size_t i, j;
    
    LOGI("Supported AT commands:");
    for (i = 0; i < cat_desc.cmd_group_num; i++) {
        for (j = 0; j < cat_desc.cmd_group[i]->cmd_num; j++) {
            LOGI("  %s - %s",
                 cat_desc.cmd_group[i]->cmd[j].name,
                 cat_desc.cmd_group[i]->cmd[j].description);
        }
    }
}

int main(void)
{
    LOGI("cAT AT Command Parser Example");
    LOGI("=============================");

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
    LOGI("Try commands: AT+VERSION?, AT+LED=1, AT+HELP");
    print_supported_commands();

    /* Start background processing tasks (also initializes cAT parser) */
    cat_uart_adapter_start_service(adapter, &cat, &cat_desc);

    /* Main loop can do other things */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

/* ============================================================================
 * Example Usage:
 *
 * Send these AT commands via UART to test:
 *
 * AT                    -> OK
 * AT+VERSION?           -> 1.0.0\r\nOK
 * AT+LED=1              -> OK (LED turned on)
 * AT+LED?               -> +LED: 1\r\nOK
 * AT+GPIO=5,1           -> OK (GPIO 5 set to high)
 * AT+GPIO?              -> +GPIO: 5,1\r\nOK
 * AT+RESET              -> OK (device resets)
 * AT+HELP               -> (prints all commands)
 * AT+LED=?              -> +LED: state(INT)\r\nOK (test command)
 *
 * ============================================================================ */
