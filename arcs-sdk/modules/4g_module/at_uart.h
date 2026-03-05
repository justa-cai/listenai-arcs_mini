/**
 * @file at_uart.h
 * @brief AT Command UART Interface
 * @details UART communication interface for 4G module AT commands
 *          Using lisa_uart driver with command/response/URC mechanism
 */

#ifndef __AT_UART_H__
#define __AT_UART_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== AT Command Argument Types ===== */

/**
 * @brief AT command argument value type
 */
typedef enum {
    AT_ARG_TYPE_STRING,     /**< String type */
    AT_ARG_TYPE_INT,        /**< Integer type */
    AT_ARG_TYPE_DOUBLE      /**< Double type (not used currently) */
} at_arg_type_t;

/**
 * @brief AT command argument value structure
 */
typedef struct {
    at_arg_type_t type;     /**< Argument type */
    union {
        struct {
            char *value;    /**< String value (dynamically allocated) */
            size_t len;     /**< String length */
        } string_val;
        int int_val;        /**< Integer value */
        double double_val;  /**< Double value */
    } data;
} at_arg_value_t;

/* ===== URC Callback ===== */

/**
 * @brief URC (Unsolicited Result Code) callback function type
 *
 * @param command URC command name (without '+' prefix)
 * @param arguments Parsed arguments array
 * @param arg_count Number of arguments
 * @param user_data User data passed during registration
 */
typedef void (*at_urc_callback_t)(const char *command, at_arg_value_t *arguments,
                                   size_t arg_count, void *user_data);

/**
 * @brief URC callback node (linked list)
 */
typedef struct at_urc_callback_node {
    at_urc_callback_t callback;         /**< Callback function */
    void *user_data;                    /**< User data */
    struct at_urc_callback_node *next;  /**< Next node */
} at_urc_callback_node_t;

/* ===== Configuration ===== */

/**
 * @brief AT UART configuration
 */
typedef struct {
    uint8_t  uart_id;         /**< UART ID (deprecated) */
    uint32_t baudrate;        /**< Baudrate: 9600, 115200, 1000000, etc. */
    uint8_t  tx_pad;          /**< TX pin PAD (deprecated) */
    uint8_t  tx_pin;          /**< TX pin number (deprecated) */
    uint8_t  rx_pad;          /**< RX pin PAD (deprecated) */
    uint8_t  rx_pin;          /**< RX pin number (deprecated) */
} at_uart_config_t;

/* ===== Initialization ===== */

/**
 * @brief Initialize AT UART
 *
 * @param uart_dev_name uart device name
 * @return 0 on success, -1 on failure
 */
int at_uart_init(const char *uart_dev_name);

/**
 * @brief Deinitialize AT UART
 *
 * @return 0 on success, -1 on failure
 */
int at_uart_deinit(void);

/* ===== Command Sending ===== */

/**
 * @brief Send AT command
 *
 * This function sends an AT command and waits for OK/ERROR response.
 *
 * @param command AT command string (e.g., "AT", "AT+CPIN?")
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @param add_crlf Add \r\n to the end of command
 * @return true on success (received OK), false on failure
 */
bool at_uart_send_command(const char *command, size_t timeout_ms, bool add_crlf);

/**
 * @brief Send AT command with data payload
 *
 * This function sends an AT command, waits for '>' prompt (if applicable),
 * then sends the data payload.
 *
 * @param command AT command string
 * @param timeout_ms Timeout in milliseconds
 * @param add_crlf Add \r\n to the end of command
 * @param data Data payload to send (can be NULL)
 * @param data_len Data payload length
 * @return true on success (received OK), false on failure
 */
bool at_uart_send_command_with_data(const char *command, size_t timeout_ms,
                                    bool add_crlf, const uint8_t *data, size_t data_len);

/**
 * @brief Get CME error code
 *
 * Returns the last CME ERROR code received from +CME ERROR: <code>
 *
 * @return CME error code (0 if no error)
 */
int at_uart_get_cme_error_code(void);

/**
 * @brief UART baudrate adaptation
 *
 * Adapts the UART baudrate from 115200 to 921600 automatically.
 * Tests AT communication at 115200, then switches to 921600.
 *
 * @return true on success, false on failure
 */
bool uart_baudrate_adapt(void);

/* ===== URC Management ===== */

/**
 * @brief Register URC callback
 *
 * Registers a callback function to be called when a URC (Unsolicited Result Code)
 * is received. Multiple callbacks can be registered.
 *
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return Callback node pointer (used for unregister), or NULL on failure
 */
at_urc_callback_node_t *at_uart_register_urc_callback(at_urc_callback_t callback, void *user_data);

/**
 * @brief Unregister URC callback
 *
 * @param node Callback node returned by at_uart_register_urc_callback()
 */
void at_uart_unregister_urc_callback(at_urc_callback_node_t *node);

/* ===== Utilities ===== */

/**
 * @brief Set debug mode
 *
 * When debug mode is enabled, all sent commands and received responses
 * are printed to the log.
 *
 * @param enable true to enable debug, false to disable
 */
void at_uart_set_debug(bool enable);

/**
 * @brief Destroy argument array
 *
 * Frees memory allocated for argument array (including string values)
 *
 * @param args Argument array
 * @param count Number of arguments
 */
void at_arg_array_destroy(at_arg_value_t *args, size_t count);

/* ===== Legacy API (Deprecated) ===== */

/** @deprecated Use at_uart_send_command() instead */
int at_uart_send(const uint8_t *data, uint16_t size);

/** @deprecated Not supported, returns -1 */
int at_uart_receive(uint8_t *data, uint16_t size, uint32_t timeout_ms);

/** @deprecated Not supported, returns 0 */
uint16_t at_uart_available(void);

/** @deprecated Minimal implementation */
int at_uart_flush(void);

/** @deprecated Use at_uart_send_command() directly */
int at_uart_send_cmd(const char *cmd);

/** @deprecated Use at_uart_send_command_with_data() directly */
int at_uart_send_cmd_with_data(const char *cmd, const uint8_t *data, uint16_t data_len);

/** @deprecated Callback type for legacy API */
typedef void (*at_uart_rx_callback_t)(uint8_t *data, int len, void *arg);

/** @deprecated Use at_uart_register_urc_callback() instead */
int at_uart_set_rx_callback(at_uart_rx_callback_t callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* __AT_UART_H__ */
