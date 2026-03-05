/**
 * @file ml307_tcp.h
 * @brief ML307 TCP Connection Management
 * @details TCP/SSL connection implementation for ML307 4G module
 */

#ifndef __ML307_TCP_H__
#define __ML307_TCP_H__

#include "at_uart.h"
#include <stddef.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bit position macros for event groups */
#ifndef BIT0
#define BIT0    (1 << 0)
#define BIT1    (1 << 1)
#define BIT2    (1 << 2)
#define BIT3    (1 << 3)
#define BIT4    (1 << 4)
#define BIT5    (1 << 5)
#define BIT6    (1 << 6)
#define BIT7    (1 << 7)
#define BIT8    (1 << 8)
#define BIT9    (1 << 9)
#define BIT10   (1 << 10)
#define BIT11   (1 << 11)
#define BIT12   (1 << 12)
#define BIT13   (1 << 13)
#define BIT14   (1 << 14)
#define BIT15   (1 << 15)
#endif

#define WEBSOCKET_CONNECT_ID        0
#define PLAYER_CONNECT_ID           1

/* ===== Event Bits ===== */
#define ML307_TCP_CONNECTED      BIT0    /**< TCP connected */
#define ML307_TCP_DISCONNECTED   BIT1    /**< TCP disconnected */
#define ML307_TCP_ERROR          BIT2    /**< TCP error */
#define ML307_TCP_SEND_COMPLETE  BIT3    /**< Send complete */
#define ML307_TCP_INITIALIZED    BIT4    /**< TCP initialized */
#define ML307_TCP_DATA_READY     BIT5    /**< Data available in recv buffer */
#define ML307_TCP_PREFETCH_AVAILABLE BIT6    /**< Data available in recv buffer */

/* ===== Timeouts ===== */
#define TCP_CONNECT_TIMEOUT_MS   10000   /**< Connect timeout (5s) */
#define TCP_SEND_TIMEOUT_MS      5000    /**< Send timeout (3s) */

/* ===== Callback Types ===== */

/**
 * @brief TCP data stream callback
 *
 * Called when data is received from remote server
 *
 * @param data Received data buffer
 * @param len Data length
 * @param user_data User data passed during registration
 */
typedef void (*tcp_stream_callback_t)(const char *data, size_t len, void *user_data);

/**
 * @brief TCP disconnect callback
 *
 * Called when TCP connection is disconnected
 *
 * @param user_data User data passed during registration
 */
typedef void (*tcp_disconnect_callback_t)(void *user_data);

/* ===== Opaque Pointer ===== */
typedef struct ml307_tcp ml307_tcp_t;

/* ===== Initialization and Destruction ===== */

/**
 * @brief Initialize ML307 TCP connection
 *
 * @param tcp_id TCP connection ID (0-5)
 * @param is_ssl Enable SSL/TLS
 * @return true on success, false on failure
 */
int ml307_tcp_init(bool is_ssl);

/**
 * @brief Deinitialize TCP connection
 *
 * @param tcp_id TCP connection ID (0-5)
 */
void ml307_tcp_deinit(int tcp_id);

/* ===== TCP Operations ===== */

/**
 * @brief Connect to remote server
 *
 * Automatically allocates a free connection ID and initializes the connection.
 * Connection IDs are shared with UDP (0-4).
 *
 * @param host Host name or IP address
 * @param port Port number
 * @param is_ssl Enable SSL/TLS
 * @return Connection ID (0-4) on success, -1 on failure
 */
bool ml307_tcp_connect(int tcp_id, const char *host, int port, bool is_ssl);

/**
 * @brief Disconnect from remote server
 *
 * Automatically deinitializes the connection after disconnecting.
 *
 * @param tcp_id TCP connection ID (0-5)
 */
int ml307_tcp_disconnect(int tcp_id);

/**
 * @brief Send data to remote server
 *
 * @param tcp_id TCP connection ID (0-5)
 * @param data Data buffer to send
 * @param length Data length
 * @return Number of bytes sent, or -1 on error
 */
int ml307_tcp_send(int tcp_id, const char *data, size_t length);

/**
 * @brief Receive data from remote server
 *
 * @param tcp_id TCP connection ID (0-5)
 * @param buffer Buffer to store received data
 * @param length Maximum bytes to receive
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return Number of bytes received, 0 if no data, -1 on error
 */
int ml307_tcp_recv(int tcp_id, char *buffer, size_t length, uint32_t timeout_ms);

/* ===== Callback Registration ===== */

/**
 * @brief Register data stream callback
 *
 * @param tcp_id TCP connection ID (0-5)
 * @param callback Callback function
 * @param user_data User data
 */
void ml307_tcp_on_stream(int tcp_id, tcp_stream_callback_t callback, void *user_data);

/**
 * @brief Register disconnect callback
 *
 * @param tcp_id TCP connection ID (0-5)
 * @param callback Callback function
 * @param user_data User data
 */
void ml307_tcp_on_disconnected(int tcp_id, tcp_disconnect_callback_t callback, void *user_data);

/* ===== Status Query ===== */

/**
 * @brief Check if TCP is connected
 *
 * @param tcp_id TCP connection ID (0-5)
 * @return true if connected, false otherwise
 */
bool ml307_tcp_connected(int tcp_id);

/**
 * @brief Get last error code
 *
 * @param tcp_id TCP connection ID (0-5)
 * @return Error code (0 if no error)
 */
int ml307_tcp_get_last_error(int tcp_id);

/**
 * @brief Start TCP prefetch task
 *
 * Starts a background task that periodically calls ml307_tcp_data_prefetch()
 * to proactively read data from the modem into recv buffers.
 *
 * @return 0 on success, -1 if task already running or creation failed
 */
int ml307_tcp_start_prefetch_task(void);

#ifdef __cplusplus
}
#endif

#endif /* __ML307_TCP_H__ */
