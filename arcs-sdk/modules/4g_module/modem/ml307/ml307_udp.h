/**
 * @file ml307_udp.h
 * @brief ML307 UDP Connection Management
 * @details UDP connection implementation for ML307 4G module
 */

#ifndef __ML307_UDP_H__
#define __ML307_UDP_H__

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

/* ===== Event Bits ===== */
#define ML307_UDP_CONNECTED      BIT0    /**< UDP connected */
#define ML307_UDP_DISCONNECTED   BIT1    /**< UDP disconnected */
#define ML307_UDP_ERROR          BIT2    /**< UDP error */
#define ML307_UDP_SEND_COMPLETE  BIT3    /**< Send complete */
#define ML307_UDP_INITIALIZED    BIT4    /**< UDP initialized */
#define ML307_UDP_DATA_AVAILABLE BIT5    /**< Data available in recv buffer */
#define ML307_UDP_DATA_READY     BIT6    /**< Data ready notification from modem */
#define ML307_UDP_PREFETCH_AVAILABLE BIT7 /**< Data prefetch complete */

/* ===== Timeouts ===== */
#define UDP_CONNECT_TIMEOUT_MS   10000   /**< Connect timeout (10s) */
#define UDP_SEND_TIMEOUT_MS      5000    /**< Send timeout (5s) */

/* ===== Maximum Packet Size ===== */
#define UDP_MAX_PACKET_SIZE      730     /**< Max UDP packet size (1460/2 for hex) */

/* ===== Callback Types ===== */

/**
 * @brief UDP data message callback
 *
 * Called when data is received from remote server
 *
 * @param data Received data buffer
 * @param len Data length
 * @param user_data User data passed during registration
 */
typedef void (*udp_message_callback_t)(const char *data, size_t len, void *user_data);

/**
 * @brief UDP disconnect callback
 *
 * Called when UDP connection is disconnected
 *
 * @param user_data User data passed during registration
 */
typedef void (*udp_disconnect_callback_t)(void *user_data);

/* ===== Opaque Pointer ===== */
typedef struct ml307_udp ml307_udp_t;

/* ===== Initialization and Destruction ===== */

/**
 * @brief Initialize ML307 UDP connection
 *
 * @return Connection ID (0-4) on success, -1 on failure
 */
int ml307_udp_init(void);

/**
 * @brief Deinitialize UDP connection
 *
 * @param udp_id UDP connection ID (0-5)
 */
void ml307_udp_deinit(int udp_id);

/* ===== UDP Operations ===== */

/**
 * @brief Connect to remote server using specified connection ID
 *
 * @param udp_id UDP connection ID (0-4)
 * @param host Host name or IP address
 * @param port Port number
 * @return true on success, false on failure
 */
bool ml307_udp_connect(int udp_id, const char *host, int port);

/**
 * @brief Disconnect from remote server
 *
 * Automatically deinitializes the connection after disconnecting.
 *
 * @param udp_id UDP connection ID (0-5)
 */
int ml307_udp_disconnect(int udp_id);

/**
 * @brief Send data to remote server
 *
 * @param udp_id UDP connection ID (0-5)
 * @param data Data buffer to send
 * @param length Data length
 * @return Number of bytes sent, or -1 on error
 */
int ml307_udp_send(int udp_id, const char *data, size_t length);

/**
 * @brief Receive data from remote server
 *
 * @param udp_id UDP connection ID (0-5)
 * @param buffer Buffer to store received data
 * @param length Maximum bytes to receive
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return Number of bytes received, 0 if no data, -1 on error
 */
int ml307_udp_recv(int udp_id, char *buffer, size_t length, uint32_t timeout_ms);

/* ===== Callback Registration ===== */

/**
 * @brief Register data message callback
 *
 * @param udp_id UDP connection ID (0-5)
 * @param callback Callback function
 * @param user_data User data
 */
void ml307_udp_on_message(int udp_id, udp_message_callback_t callback, void *user_data);

/**
 * @brief Register disconnect callback
 *
 * @param udp_id UDP connection ID (0-5)
 * @param callback Callback function
 * @param user_data User data
 */
void ml307_udp_on_disconnected(int udp_id, udp_disconnect_callback_t callback, void *user_data);

/* ===== Status Query ===== */

/**
 * @brief Check if UDP is connected
 *
 * @param udp_id UDP connection ID (0-5)
 * @return true if connected, false otherwise
 */
bool ml307_udp_connected(int udp_id);

/**
 * @brief Get last error code
 *
 * @param udp_id UDP connection ID (0-5)
 * @return Error code (0 if no error)
 */
int ml307_udp_get_last_error(int udp_id);

/**
 * @brief Start UDP prefetch task
 *
 * Starts a background task that periodically calls ml307_udp_data_prefetch()
 * to proactively read data from the modem into recv buffers.
 *
 * @return 0 on success, -1 if task already running or creation failed
 */
int ml307_udp_start_prefetch_task(void);

/**
 * @brief Stop UDP prefetch task
 *
 * Stops the background UDP data prefetch task.
 */
void ml307_udp_stop_prefetch_task(void);

#ifdef __cplusplus
}
#endif

#endif /* __ML307_UDP_H__ */
