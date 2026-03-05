/*
 * Copyright (c) 2024 ListenAI
 * SPDX-License-Identifier: MIT
 *
 * cAT UART Adapter for ARCS SDK
 * 
 * This module provides UART transport layer integration for the cAT AT command parser.
 */

#ifndef CAT_UART_ADAPTER_H
#define CAT_UART_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "../src/cat.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief cAT UART adapter configuration
 */
typedef struct {
    const char *uart_device_name;   /**< UART device name (e.g., "uart1") */
    uint32_t baudrate;              /**< Baud rate (e.g., 115200) */
} cat_uart_config_t;

/**
 * @brief cAT UART adapter context
 */
typedef struct cat_uart_adapter cat_uart_adapter_t;

/**
 * @brief Initialize the cAT UART adapter
 *
 * @param config Pointer to configuration structure
 * @return Pointer to adapter context on success, NULL on failure
 */
cat_uart_adapter_t *cat_uart_adapter_init(const cat_uart_config_t *config);

/**
 * @brief Deinitialize the cAT UART adapter
 *
 * @param adapter Pointer to adapter context
 */
void cat_uart_adapter_deinit(cat_uart_adapter_t *adapter);

/**
 * @brief Process cAT service (should be called periodically)
 *
 * This function reads data from UART and feeds it to the cAT parser.
 * It should be called periodically from a task or main loop.
 *
 * @param adapter Pointer to adapter context
 * @param cat Pointer to cAT object
 * @return Number of bytes processed, or negative error code
 */
int cat_uart_adapter_process(cat_uart_adapter_t *adapter, struct cat_object *cat);

/**
 * @brief Start the cAT UART processing tasks
 *
 * Creates background tasks that automatically call cat_uart_adapter_process().
 * This function also initializes the cAT object with the provided descriptor.
 * Stack size and priorities for the RX and processing tasks are configured via Kconfig
 * options: CONFIG_SDK_MODULE_CAT_UART_RX_TASK_STACK_SIZE,
 * CONFIG_SDK_MODULE_CAT_UART_RX_TASK_PRIORITY,
 * CONFIG_SDK_MODULE_CAT_UART_PROC_TASK_STACK_SIZE,
 * CONFIG_SDK_MODULE_CAT_UART_PROC_TASK_PRIORITY.
 *
 * @param adapter Pointer to adapter context
 * @param cat Pointer to cAT object
 * @param desc Pointer to cAT descriptor
 * @return 0 on success, negative error code on failure
 */
int cat_uart_adapter_start_service(cat_uart_adapter_t *adapter,
                                 struct cat_object *cat,
                                 const struct cat_descriptor *desc);

/**
 * @brief Stop the cAT UART processing task
 *
 * @param adapter Pointer to adapter context
 * @return 0 on success, negative error code on failure
 */
int cat_uart_adapter_stop_service(cat_uart_adapter_t *adapter);

/**
 * @brief Send unsolicited response
 *
 * Sends an unsolicited response (URC) through the UART.
 *
 * @param adapter Pointer to adapter context
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int cat_uart_adapter_send_urc(cat_uart_adapter_t *adapter, const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CAT_UART_ADAPTER_H */