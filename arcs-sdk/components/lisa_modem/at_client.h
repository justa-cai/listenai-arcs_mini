/**
 * @file at_client.h
 * @brief AT Command Client - Multi-instance AT Protocol Handler
 * @details Handles AT command sending/response, URC parsing and dispatch,
 *          argument parsing, hex encoding/decoding.
 *          Decoupled from transport - bind any transport implementation.
 */

#ifndef __AT_CLIENT_H__
#define __AT_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "transport/at_transport.h"

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

typedef bool (*at_urc_match_fn)(const char *command, at_arg_value_t *arguments,
                                size_t arg_count, void *user_data);

/**
 * @brief URC callback node (linked list)
 */
typedef struct at_urc_callback_node {
    at_urc_callback_t callback;         /**< Callback function */
    void *user_data;                    /**< User data */
    struct at_urc_callback_node *next;  /**< Next node */
} at_urc_callback_node_t;

/* ===== Line Stream Claim ===== */

typedef enum {
    AT_LINE_STREAM_PASS = 0,
    AT_LINE_STREAM_CLAIM,
    AT_LINE_STREAM_NEED_MORE,
} at_line_stream_claim_t;

typedef at_line_stream_claim_t (*at_line_stream_claim_fn)(const uint8_t *line_prefix,
                                                          size_t prefix_len,
                                                          bool line_complete,
                                                          void *user_data,
                                                          void **claim_ctx);

typedef int (*at_line_stream_consume_fn)(void *claim_ctx, const uint8_t *data,
                                         size_t len, bool line_complete,
                                         void *user_data);

typedef void (*at_line_stream_finish_fn)(void *claim_ctx, bool success, void *user_data);

typedef struct {
    at_line_stream_claim_fn claim;
    at_line_stream_consume_fn consume;
    at_line_stream_finish_fn finish;
} at_line_stream_handler_t;

/* ===== AT Client ===== */

typedef struct at_client at_client_t;

/**
 * @brief AT client configuration
 */
typedef struct {
    size_t rx_buf_initial_size;    /**< Initial RX buffer size (default: 512) */
    size_t resp_buf_size;          /**< Response buffer size (default: 512) */
    uint8_t task_priority;         /**< Processing task priority (default: 10) */
    uint16_t task_stack_size;      /**< Processing task stack size (default: 2048) */
    uint16_t rx_task_delay_ms;     /**< Polling delay for read-task transports (default: 10ms) */
} at_client_config_t;

#define AT_CLIENT_CONFIG_DEFAULT() { \
    .rx_buf_initial_size = 512,      \
    .resp_buf_size = 512,            \
    .task_priority = 10,             \
    .task_stack_size = 2048,         \
    .rx_task_delay_ms = 10,          \
}

/* ===== Lifecycle ===== */

/**
 * @brief Create an AT client instance
 *
 * @param config Client configuration (NULL for defaults)
 * @return Client instance, or NULL on failure
 */
at_client_t *at_client_create(const at_client_config_t *config);

/**
 * @brief Destroy an AT client instance
 *
 * @param client Client instance
 */
void at_client_destroy(at_client_t *client);

/**
 * @brief Bind a transport to the client and start processing
 *
 * @param client Client instance
 * @param transport Transport to bind
 * @return 0 on success, negative on error
 */
int at_client_bind(at_client_t *client, at_transport_t *transport);

/* ===== Command Sending ===== */

/**
 * @brief Send AT command and wait for OK/ERROR
 *
 * @param client Client instance
 * @param command AT command string (e.g., "AT", "AT+CPIN?")
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @param add_crlf Add \r\n to the end of command
 * @return true on success (received OK), false on failure
 */
bool at_client_send_cmd(at_client_t *client, const char *command,
                        size_t timeout_ms, bool add_crlf);

/**
 * @brief Send AT command with data payload
 *
 * @param client Client instance
 * @param command AT command string
 * @param timeout_ms Timeout in milliseconds
 * @param add_crlf Add \r\n to the end of command
 * @param data Data payload to send (can be NULL)
 * @param data_len Data payload length
 * @return true on success (received OK), false on failure
 */
bool at_client_send_cmd_with_data(at_client_t *client, const char *command,
                                  size_t timeout_ms, bool add_crlf,
                                  const uint8_t *data, size_t data_len);

uint32_t at_client_get_last_cmd_wait_ms(at_client_t *client);
uint32_t at_client_get_last_ok_wait_resume_ms(at_client_t *client);

/**
 * @brief Send AT command and wait for a specific URC response
 *
 * Sends the command, then waits until both of these conditions are met:
 * 1. AT layer success: final OK is received
 * 2. Business layer success: a URC matching expect_urc is received
 *
 * The OK and expected URC may arrive in either order. ERROR/CME ERROR or timeout
 * cause failure. Returns the parsed URC arguments to the caller. Caller must free
 * arguments with at_arg_array_destroy().
 *
 * @param client Client instance
 * @param command AT command string (e.g., "AT+CSQ")
 * @param expect_urc Expected URC command name without '+' (e.g., "CSQ")
 * @param out_args Output: parsed argument array (caller frees)
 * @param out_count Output: argument count
 * @param timeout_ms Timeout in milliseconds
 * @param add_crlf Add \r\n to the end of command
 * @return true if both final OK and the expected URC were received, false on timeout/error
 */
bool at_client_send_cmd_wait_urc(at_client_t *client, const char *command,
                                  const char *expect_urc,
                                  at_arg_value_t **out_args, size_t *out_count,
                                  size_t timeout_ms, bool add_crlf);

bool at_client_send_cmd_wait_urc_match(at_client_t *client, const char *command,
                                       const char *expect_urc,
                                       at_urc_match_fn match_fn, void *match_user_data,
                                       at_arg_value_t **out_args, size_t *out_count,
                                       size_t timeout_ms, bool add_crlf);

/* ===== Command Descriptor (LwCELL-style) ===== */

/**
 * @brief AT command response parse function
 *
 * Called when the expected URC is received, with parsed arguments.
 * The function should extract values from arguments into user-provided output.
 *
 * @param args Parsed argument array
 * @param count Number of arguments
 * @param user_data User-provided output pointer
 * @return true if parsing succeeded, false if arguments are invalid
 */
typedef bool (*at_cmd_parse_fn)(at_arg_value_t *args, size_t count, void *user_data);

/**
 * @brief AT command descriptor
 *
 * Describes an AT command, its expected URC response, parse callback, and timeout.
 * Can be used with compound literal for inline usage.
 */
typedef struct {
    const char *cmd;            /**< AT command string (e.g., "AT+CSQ") */
    const char *expect_urc;     /**< Expected URC name without '+' (e.g., "CSQ"), NULL for OK-only */
    at_cmd_parse_fn parse;      /**< Parse callback (NULL to skip parsing) */
    uint32_t timeout_ms;        /**< Timeout in milliseconds */
} at_cmd_desc_t;

/**
 * @brief Execute AT command with descriptor
 *
 * Sends the command, waits for the expected URC (if specified), invokes the
 * parse callback, and waits for OK/ERROR. This is the recommended high-level
 * API for AT command execution.
 *
 * @param client Client instance
 * @param desc Command descriptor
 * @param user_data Passed to parse callback (typically pointer to output struct)
 * @return true on success (command OK + URC matched + parse succeeded), false otherwise
 *
 * @code
 * // Example: Query signal quality
 * static bool parse_csq(at_arg_value_t *args, size_t count, void *user_data) {
 *     int *out = (int *)user_data;
 *     if (count < 2 || args[0].type != AT_ARG_TYPE_INT) return false;
 *     out[0] = args[0].data.int_val;  // rssi
 *     out[1] = args[1].data.int_val;  // ber
 *     return true;
 * }
 *
 * int result[2];
 * at_client_exec_cmd(client, &(at_cmd_desc_t){
 *     .cmd = "AT+CSQ", .expect_urc = "CSQ",
 *     .parse = parse_csq, .timeout_ms = 1000,
 * }, result);
 * @endcode
 */
bool at_client_exec_cmd(at_client_t *client, const at_cmd_desc_t *desc, void *user_data);

/**
 * @brief Execute AT command and capture a plain-text response line
 *
 * Suitable for commands whose main payload is returned as a raw line before OK,
 * such as CGMM/CGMI/CGMR on some modems.
 *
 * @param client Client instance
 * @param command AT command string
 * @param response Output buffer for the plain-text line
 * @param size Output buffer size
 * @param timeout_ms Timeout in milliseconds
 * @return true on success, false otherwise
 */
bool at_client_exec_text_cmd(at_client_t *client, const char *command,
                             char *response, size_t size, uint32_t timeout_ms);

/**
 * @brief Execute AT command with formatted command string
 *
 * Same as at_client_exec_cmd(), but the command string supports printf-style formatting.
 *
 * @param client Client instance
 * @param desc Command descriptor (cmd field is used as format string)
 * @param user_data Passed to parse callback
 * @param ... Format arguments for desc->cmd
 * @return true on success, false otherwise
 *
 * @code
 * at_client_exec_cmdf(client, &(at_cmd_desc_t){
 *     .cmd = "AT+MDNSGIP=\"%s\"", .expect_urc = "MDNSGIP",
 *     .parse = parse_dns, .timeout_ms = 10000,
 * }, &out, domain);
 * @endcode
 */
bool at_client_exec_cmdf(at_client_t *client, const at_cmd_desc_t *desc,
                          void *user_data, ...);

/**
 * @brief Get last CME error code
 *
 * @param client Client instance
 * @return CME error code (0 if no error)
 */
int at_client_get_cme_error(at_client_t *client);

/* ===== URC Management ===== */

/**
 * @brief Register URC callback
 *
 * @param client Client instance
 * @param callback Callback function
 * @param user_data User data to pass to callback
 * @return Callback node pointer, or NULL on failure
 */
at_urc_callback_node_t *at_client_register_urc(at_client_t *client,
                                                at_urc_callback_t callback,
                                                void *user_data);

/**
 * @brief Unregister URC callback
 *
 * @param client Client instance
 * @param node Callback node returned by at_client_register_urc()
 */
void at_client_unregister_urc(at_client_t *client, at_urc_callback_node_t *node);

/**
 * @brief Register or clear an optional line-stream claim handler
 *
 * The handler inspects the beginning of each AT line and may claim it for
 * streaming consumption. Claimed lines bypass generic argument parsing.
 *
 * @param client Client instance
 * @param handler Handler table, or NULL to clear
 * @param user_data User data passed to the handler callbacks
 * @return 0 on success, negative on error
 */
int at_client_set_line_stream_handler(at_client_t *client,
                                      const at_line_stream_handler_t *handler,
                                      void *user_data);

/* ===== Utilities ===== */

/**
 * @brief Set debug mode
 *
 * AT client I/O logs are disabled by default.
 *
 * @param client Client instance
 * @param enable true to enable debug, false to disable
 */
void at_client_set_debug(at_client_t *client, bool enable);

/**
 * @brief Set receive task polling delay.
 *
 * Only affects transports using the internal RX task. Passing 0 disables the
 * extra delay and makes the task poll as fast as the transport timeout allows.
 *
 * @param client Client instance
 * @param delay_ms Delay in milliseconds
 * @return 0 on success, negative on error
 */
int at_client_set_rx_task_delay(at_client_t *client, uint16_t delay_ms);

/**
 * @brief Get receive task polling delay.
 *
 * @param client Client instance
 * @return Delay in milliseconds, or 0 when client is NULL
 */
uint16_t at_client_get_rx_task_delay(at_client_t *client);

/**
 * @brief Get the bound transport
 *
 * @param client Client instance
 * @return Transport pointer, or NULL if not bound
 */
at_transport_t *at_client_get_transport(at_client_t *client);

/**
 * @brief Destroy argument array
 *
 * @param args Argument array
 * @param count Number of arguments
 */
void at_arg_array_destroy(at_arg_value_t *args, size_t count);

/**
 * @brief Encode binary data to hex string
 *
 * @param data Input binary data
 * @param length Length of input data
 * @param out_len Output hex string length
 * @return Hex string (caller must free), or NULL on failure
 */
char *at_encode_hex(const char *data, size_t length, size_t *out_len);

/**
 * @brief Decode hex string to binary data
 *
 * @param hex_str Input hex string
 * @param hex_len Length of hex string
 * @param out_len Output binary data length
 * @return Binary data (caller must free), or NULL on failure
 */
char *at_decode_hex(const char *hex_str, size_t hex_len, size_t *out_len);

/* ===== Baudrate Adaptation (UART-specific helper) ===== */

/**
 * @brief Sync UART transport to a baudrate where AT responds
 *
 * Tries AT at two candidate baudrates and leaves the transport configured
 * to the baudrate that successfully responds. Does not reconfigure the modem.
 *
 * @param client Client instance
 * @param init_baud Primary baudrate to try first
 * @param fallback_baud Secondary baudrate to try if the primary fails
 * @return true on success, false on failure
 */
bool at_client_uart_baudrate_sync(at_client_t *client,
                                  uint32_t init_baud, uint32_t fallback_baud);

/**
 * @brief Query current UART transport baudrate
 *
 * Requires a UART transport to be bound.
 *
 * @param client Client instance
 * @param baudrate Output current baudrate
 * @return true on success, false on failure
 */
bool at_client_uart_get_baudrate(at_client_t *client, uint32_t *baudrate);

/**
 * @brief Adapt UART baudrate from initial to target
 *
 * Tests AT communication at current baudrate, then switches to target.
 * Requires a UART transport to be bound.
 *
 * @param client Client instance
 * @param init_baud Initial baudrate to try
 * @param target_baud Target baudrate
 * @return true on success, false on failure
 */
bool at_client_uart_baudrate_adapt(at_client_t *client,
                                   uint32_t init_baud, uint32_t target_baud);

#ifdef __cplusplus
}
#endif

#endif /* __AT_CLIENT_H__ */
