#pragma once

#include "wifi_manager/wifi_manager_ops.h"
#include "wifi_manager/wifi_manager_wifi_ops.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

// 使用统一的类型定义
typedef wifi_mgr_wifi_encryption_mode_t wifi_manager_encryption_mode_t;
typedef wifi_mgr_wifi_sta_config_t wifi_mgr_sta_config_t;
typedef wifi_mgr_wifi_scan_info_t wifi_mgr_scan_info_t;

/**
 * @brief WiFi scan list struct
 */
typedef struct {
    wifi_mgr_scan_info_t *ap_info;
    uint32_t count;
} wifi_mgr_scan_ap_list_t;

typedef struct {
    uint32_t interval_ms;
} wifi_mgr_autoconn_config_t;

typedef enum {
    WIFI_MGR_STA_CONNECTED = 0,
    WIFI_MGR_STA_CONNECTING,
    WIFI_MGR_STA_DISCONNECTED,
    WIFI_MGR_STA_CONNECT_FAILED,
    WIFI_MGR_STA_MAX = 0xFF
} wifi_mgr_connection_status_t;

typedef struct {
    wifi_mgr_connection_status_t status;
    wifi_mgr_sta_config_t *sta_info;
    int reason;      /* IEEE 802.11 reason code */
} wifi_mgr_connection_info_t;


/**
 * @brief WiFi event callback handler type. 
 */
typedef void (*wifi_mgr_connection_cb_t)(wifi_mgr_connection_info_t *connection_info, void *arg);


/**
 * @brief WiFi scan done event handler type
 * if ap_num > 0: number of scanned aps. 
 * if ap_num < 0: the error code is negative number of @see csk_wifi_event_t. 
 */
typedef void (*wifi_mgr_scan_done_cb_t)(wifi_mgr_scan_info_t *aps_info, int ap_num, void *arg);

/**
 * @brief WiFi storage searching mode
 */
typedef enum {
    /**
     * This is meant to match nothing and it will cause return all
     * the items saved in the WiFi NVS storage.
	 */
    SEARCH_ALL =            BIT(0),

    /**
     * This is meant to match the SSID field and it will cause return all
     * the items have the same SSID field from the WiFi NVS storage.
	 */
    SEARCH_BY_SSID =        BIT(1),

    /**
     * This is meant to match the BSSID field and it will cause return all
     * the items have the same BSSID field from the WiFi NVS storage.
	 */
    SEARCH_BY_BSSID =       BIT(2),


    /**
     * This is meant to match the Password field and it will cause return all
     * the items have the same Password field from the WiFi NVS storage.
	 */
    SEARCH_BY_PWD =         BIT(3),

    /**
     * This is meant to match the Channel field and it will cause return all
     * the items have the same Channel field from the WiFi NVS storage.
	 */
    SEARCH_BY_CHANNEL =     BIT(4),

    /**
     * This is meant to match the Encryption field and it will cause return all
     * the items have the same Encryption field from the WiFi NVS storage.
	 */
    SEARCH_BY_ENCRYPTION =  BIT(5),
} wifi_mgr_storage_search_mode_t;


/**
 * @brief WiFi manager initialize
 *
 * @param mem_ops[in] Pointer to memory operations structure
 * @param os_ops[in] Pointer to operating system operations structure
 *
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_init(wifi_mgr_ops_t *ops);

/**
 * @brief Release resources allocated using wifi_mgr_init()
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_deinit();

/**
 * @brief Enable wifi manager station mode.
 * 
 * @return 0 if successful, negative errno code on failure. 
 */
int wifi_mgr_sta_enable();

/**
 * @brief Disable wifi manager station mode.
 * @note This function will call wifi_mgr_auto_connect_stop() to make sure auto connect is stopped.
 * 
 * @return 0 if successful, negative errno code on failure. 
 */
int wifi_mgr_sta_disable();

/*
 * @brief Check if wifi manager station mode is enabled.
 * 
 * @return true if wifi manager station mode is enabled, false otherwise.
*/
bool wifi_mgr_sta_is_enable(void);

/**
 * @brief Start a WiFi connection request as a WiFi station device
 *
 * This function will start a WiFi station connect request to the driver core
 * and the WiFi driver thread will start the real WiFi connection request.
 *
 * @note This function could only call while WiFi is in Station disconnected status
 *
 * @param sta_config[in/out] Pointer to WiFi Station configuration structure
 * @param asynchronous[in] Asynchronous/synchronous mode (Must be false now since asynchronous is not available)
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_sta_connect(wifi_mgr_sta_config_t *sta_config, bool asynchronous);

/**
 * @brief Add WiFi Station status change listener
 * 
 * This function add WiFi Sattion status change handler into driver core, once WiFi Station status
 * changed, the event hanlder will be called.
 *
 * @param connection_cb[in] WiFi Station connection event handler
 * @param arg[in] User argument
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_cb_t connection_cb, void *arg);

/**
 * @brief Remove WiFi Station connection event handler 
 *
 * This function removes the WiFi Station connection event handler added by 
 * `wifi_mgr_sta_add_connection_cb()`
 *
 * @param connection_cb[in] WiFi Station connection event handler
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_sta_remove_connection_cb(wifi_mgr_connection_cb_t connection_cb);


/**
 * @brief WiFi Station disconnect from a connected AP 
 *
 * @param asynchronous[in] Asynchronous/synchronous mode (Must be false now since asynchronous is not available)
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_sta_disconnect(bool asynchronous);

/**
 * @brief Get WiFi connected information (AP's ssid, bssid, pwd ...)
 *
 * @note This function could only call while WiFi device is in Station connected status
 *
 * @param sta_info[out] Pointer to WiFi Station information(configuration) structure
 *
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_sta_get_connected_info(wifi_mgr_sta_config_t *sta_info);

/**
 * @brief Scan the neighboring AP device
 *
 * This function will start a Scan request and return the neighboring AP device information
 * and the device number could be accessed in the return value.
 *
 * @param ap_info[out] Pointer to WiFi AP information structure
 * @param size[in] Size of the ap_info array number
 * @param asynchronous[in] Asynchronous/synchronous mode (Must be false now since asynchronous is not available)
 *
 * @return The number of AP information, negative errno code on failure.
 */
int wifi_mgr_scan_ap(wifi_mgr_scan_info_t *ap_info, uint32_t size, bool asynchronous);

/**
 * @brief Add WiFi scan done event handler 
 *
 * This function add WiFi scan done event handler into driver core, once WiFi scan done event
 * occurs, the scan done event handler will be called.
 *
 * @param scandone_cb[in] WiFi scan done event handler
 * @param arg[in] User argument
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_add_scan_done_cb(wifi_mgr_scan_done_cb_t scandone_cb, void *arg);

/**
 * @brief Remove WiFi scan done event handler
 *
 * This function removes the WiFi scan done event event handler added by 
 * `wifi_mgr_add_scan_done_cb()`
 *
 * @param connected_cb[in] WiFi scan done event event handler
 *
 * @return 0 if successful, negative errno code and positive csk_wifi result code on failure.
 */
int wifi_mgr_remove_scan_done_cb(wifi_mgr_scan_done_cb_t scandone_cb);

/**
 * @brief Save a AP device(router) information(ssid, bssid, password, etc...) in the WiFi NVS storage
 * 
 * @note WiFi NVS storage does not allow 2 items have the same ssid, bssid fields. It means
 *       that the new AP device information item has the same those fields with the old item saved
 *       in the WiFi NVS storage will not save successfully.
 *
 * @param ap_info[in] Pointer to WiFi AP information structure
 *
 * @return 0 if successful, negative errno code on failure. return -EEXIST when ap_info was existed.
 */
int wifi_mgr_storage_save_ap(wifi_mgr_sta_config_t *ap_info);

/**
 * @brief Delete a AP device(router) information(ssid, bssid, password, etc...) from the WiFi NVS storage
 * 
 * @note This function will successful only when at least match ssid, bssid fields.
 *
 * @param ap_info[in] Pointer to WiFi AP information structure
 *
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_storage_delete_ap(wifi_mgr_sta_config_t *ap_info);

/**
 * @brief Search a AP device(router) information(ssid, bssid, password, etc...) from the the storage
 * 
 * This function will use the specified searching mode to search the AP device information item from the
 * WiFi info list saved in the storage. The searching mode defined in the `wifi_mgr_storage_search_mode_t`
 * could be use individually or as combination (use ' | ' as separator) except `SEARCH_ALL` mode.
 * 
 * @note When the searching mode is `SEARCH_ALL`, the `target` parameter should be NULL, and if combination searching
 *       mode is used, the `target` parameter type must be `wifi_mgr_sta_config_t*`.
 *
 * @param matched_list[out] Pointer to the matched AP information list
 * @param max_count[in] The maximum number of items that matched_list can hold
 * @param search_modes[in] Searching mode
 * @param target[in] Target match item (type should be char*, int*, wifi_mgr_sta_config_t*)
 *
 * @return The number of matched items found, or negative errno code on failure.
 */
int wifi_mgr_storage_search_ap(wifi_mgr_sta_config_t *matched_list, int max_count, wifi_mgr_storage_search_mode_t search_modes, void* target);

/**
 * @brief Start WiFi auto-connect to neighboring AP device
 * 
 * TODO: Add auto connect algorithm description
 * 
 * @param autoconn_config[in] Pointer to WiFi auto connect configuration structure
 *
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_auto_connect_start(wifi_mgr_autoconn_config_t *autoconn_config);

/**
 * @brief Stop WiFi auto-connect to neighboring AP device
 *
 * @return 0 if successful, negative errno code on failure.
 */
int wifi_mgr_auto_connect_stop();

/**
 * @brief Get WiFi connection status
 * 
 * @return connection status of `wifi_mgr_connection_status_t`.
 */
wifi_mgr_connection_status_t wifi_mgr_sta_get_status();

#ifdef __cplusplus
}
#endif
