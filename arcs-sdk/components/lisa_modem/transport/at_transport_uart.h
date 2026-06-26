/**
 * @file at_transport_uart.h
 * @brief UART Transport Implementation for AT Commands
 */

#ifndef __AT_TRANSPORT_UART_H__
#define __AT_TRANSPORT_UART_H__

#include "at_transport.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART transport configuration
 */
typedef struct {
    uint32_t baudrate;          /**< Initial baudrate */
    uint16_t rx_buf_size;       /**< Per-buffer size for circular receive */
    uint16_t rx_buf_count;      /**< Number of circular buffers */
} at_transport_uart_config_t;

#define AT_TRANSPORT_UART_CONFIG_DEFAULT() { \
    .baudrate = 115200,                      \
    .rx_buf_size = 512,                      \
    .rx_buf_count = 60,                      \
}

/**
 * @brief Create a UART transport instance
 *
 * @param uart_dev_name UART device name (e.g., "uart2")
 * @param config UART configuration (NULL for defaults)
 * @return Transport instance, or NULL on failure
 */
at_transport_t *at_transport_uart_create(const char *uart_dev_name,
                                          const at_transport_uart_config_t *config);

/**
 * @brief Destroy a UART transport instance
 *
 * @param transport Transport instance to destroy
 */
void at_transport_uart_destroy(at_transport_t *transport);

#ifdef __cplusplus
}
#endif

#endif /* __AT_TRANSPORT_UART_H__ */
