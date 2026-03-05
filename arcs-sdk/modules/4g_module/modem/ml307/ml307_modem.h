/**
 * @file ml307_modem.h
 * @brief ML307 4G Module modem
 * @details High-level interface for ML307 4G module management
 */

#ifndef __ML307_modem_H__
#define __ML307_modem_H__

#include "at_uart.h"
#include "ml307_tcp.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Network Status ===== */
typedef enum {
    NETWORK_STATUS_DISCONNECTED,    /**< Not registered */
    NETWORK_STATUS_REGISTERED_HOME, /**< Registered, home network */
    NETWORK_STATUS_SEARCHING,       /**< Searching for network */
    NETWORK_STATUS_DENIED,          /**< Registration denied */
    NETWORK_STATUS_UNKNOWN,         /**< Unknown */
    NETWORK_STATUS_REGISTERED_ROAMING, /**< Registered, roaming */
    NETWORK_STATUS_READY,           /**< Network ready with IP */
    NETWORK_STATUS_ERROR            /**< Error */
} network_status_t;

/* ===== Opaque Pointer ===== */
typedef struct ml307_modem ml307_modem_t;

/* ===== Initialization ===== */

/**
 * @brief Initialize ML307 modem
 *
 * Initializes AT UART and sets up ML307 module
 *
 * @return true on success, false on failure
 */
bool ml307_modem_init(const char *uart_dev);

/**
 * @brief Deinitialize ML307 modem
 */
bool ml307_modem_deinit(void);

/* ===== Module Control ===== */

/**
 * @brief Reboot ML307 module
 */
void ml307_modem_reboot(void);

/**
 * @brief Set sleep mode
 *
 * @param enable true to enable sleep, false to disable
 * @param delay_seconds Delay before entering sleep (0 for immediate)
 * @return true on success, false on failure
 */
bool ml307_modem_set_sleep_mode(bool enable, int delay_seconds);

/* ===== Network Management ===== */

/**
 * @brief Wait for network ready
 *
 * Waits for network registration and IP address assignment
 *
 * @param timeout_ms Timeout in milliseconds
 * @return Network status
 */
network_status_t ml307_network_check(void);

/**
 * @brief Check if network is ready
 *
 * @return true if network is ready, false otherwise
 */
bool ml307_modem_is_network_ready(void);

/**
 * @brief Get network status
 *
 * @return Network status
 */
network_status_t ml307_modem_get_network_status(void);

/* ===== Connection ID Management ===== */

/**
 * @brief Allocate a free connection ID
 *
 * Connection IDs are shared between TCP and UDP (0-4)
 *
 * @return Connection ID (0-4) on success, -1 if no free ID available
 */
int ml307_modem_alloc_connect_id(void);

/**
 * @brief Free a connection ID
 *
 * @param connect_id Connection ID to free (0-4)
 */
void ml307_modem_free_connect_id(int connect_id);

/* ===== Module Information ===== */

/**
 * @brief Get module IMEI
 *
 * @param imei Buffer to store IMEI (at least 16 bytes)
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_get_imei(char *imei, size_t size);

/**
 * @brief Get SIM ICCID
 *
 * @param iccid Buffer to store ICCID (at least 21 bytes)
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_get_iccid(char *iccid, size_t size);

/**
 * @brief Get signal quality (CSQ)
 *
 * @param rssi Pointer to store RSSI value (0-31, 99=unknown)
 * @param ber Pointer to store BER value (0-7, 99=unknown)
 * @return true on success, false on failure
 */
bool ml307_modem_get_signal_quality(int *rssi, int *ber);

/**
 * @brief Get module revision
 *
 * @param revision Buffer to store module revision string
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_get_module_revision(char *revision, size_t size);

/**
 * @brief Get carrier name
 *
 * @param carrier Buffer to store carrier name
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_get_carrier_name(char *carrier, size_t size);

/**
 * @brief Print module information
 *
 * Queries and prints IMEI, ICCID, CSQ, module revision, and carrier info
 */
void ml307_modem_print_module_info(void);

/* ===== Query Functions ===== */

/**
 * @brief Query SIM card PIN status (AT+CPIN?)
 *
 * @param status Buffer to store PIN status (e.g., "READY", "SIM PIN")
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_query_cpin(char *status, size_t size);

/**
 * @brief Query network registration status (AT+CEREG?)
 *
 * @param n Pointer to store URC mode
 * @param stat Pointer to store registration status
 * @return true on success, false on failure
 */
bool ml307_modem_query_cereg(int *n, int *stat);

/**
 * @brief Query PDP context status (AT+MIPCALL?)
 *
 * @param cid Pointer to store context ID (can be NULL)
 * @param status Pointer to store status (can be NULL)
 * @param ip Buffer to store IP address (can be NULL)
 * @param ip_size Size of IP buffer
 * @return true on success, false on failure
 */
bool ml307_modem_query_mipcall(int *cid, int *status, char *ip, size_t ip_size);

/**
 * @brief Query IP address (AT+CGPADDR)
 *
 * Universal command that works with all modules
 *
 * @param ip Buffer to store IP address (at least 16 bytes)
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_modem_query_cgpaddr(char *ip, size_t size);

/* ===== Utilities ===== */

/**
 * @brief Reset all connections
 *
 * Closes all HTTP and TCP connections
 */
void ml307_modem_reset_connections(void);

/* ===== DNS Resolution ===== */

/**
 * @brief Resolve domain name to IP address using ML307
 *
 * Uses AT+MDNSGIP command to perform DNS resolution
 *
 * @param domain Domain name to resolve (e.g., "www.baidu.com")
 * @param ip_addr Buffer to store resolved IP address (at least 16 bytes)
 * @param size Size of buffer
 * @return true on success, false on failure
 */
bool ml307_dns_resolve(const char *domain, char *ip_addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __ML307_modem_H__ */
