#ifndef __LS_WIFI_TYPE_H_
#define __LS_WIFI_TYPE_H_

#pragma once

#include <stdbool.h>
#include "ls_err.h"

#ifndef CFG_STA_MAX
#define CFG_STA_MAX (4)
#endif

#define VIF_MAX (1)

#define MAX_AP_SCAN (32)

#ifndef __PACKED
#define __PACKED __attribute__ ((__packed__))
#endif

/// Interface index
typedef enum
{
    /// DEFAULT interface index
    WIFI_VIF_DEFAULT_IDX = 0,
    /// SNIFFER interface index
    WIFI_VIF_SNIFFER_IDX = 0,
    /// STA interface index
    WIFI_VIF_STA_IDX = 0,
#if VIF_MAX > 1
    /// AP interface index
    WIFI_VIF_AP_IDX,
#else
    WIFI_VIF_AP_IDX = WIFI_VIF_STA_IDX,
#endif
    WLIF_IDX_MAX = 2,
} wifi_vif_idx_e;

/// wifi interface mode
typedef enum
{
    WIFI_MODE_NULL = 0,    /**< null mode */
    WIFI_MODE_STA,         /**< WiFi station mode */
    WIFI_MODE_AP,          /**< WiFi soft-AP mode */
    WIFI_MODE_SNIFFER,     /**< WiFi sniffer mode */
} wifi_mode_e;

/// WiFi Channel Band
enum wifi_band
{
    WIFI_BAND_2G4,   /** 2.4GHz Band */
    WIFI_BAND_5G,    /** 5GHz band */
    WIFI_BAND_MAX,   /** Number of bands */
};

/// WiFi Channel Bandwidth
typedef enum
{
    /// 20MHz BW
    WIFI_BW_20,
    /// 40MHz BW
    WIFI_BW_40,
    /// 80MHz BW
    WIFI_BW_80,
    /// 160MHz BW
    WIFI_BW_160,
    /// 80+80MHz BW
    WIFI_BW_80P80,
    /// Reserved BW
    WIFI_BW_OTHER,
} wifi_bandwidth_e;

typedef enum
{
    WIFI_SEC_AUTO = 0,              /**< WiFi automatically detect the security type */
    WIFI_SEC_OPEN,                  /**< Open system. */
    WIFI_SEC_WEP,                   /**< WEP security, **it's unsafe security, please don't use it** */
    WIFI_SEC_WPA_PSK,               /**< WPA TKIP/AES security */
    WIFI_SEC_WPA2_PSK,              /**< WPA2 AES */
    WIFI_SEC_WPA_PSK_WPA2_PSK,      /**< WPA or WPA2 mixed */
    WIFI_SEC_WPA_ENTERPRISE,        /**< WPA Enterprise */
    WIFI_SEC_WPA3_SAE,              /**< WPA3 SAE */
    WIFI_SEC_WPA2_PSK_WPA3_SAE,     /**< WPA3 SAE or WPA2 AES */
    WIFI_SEC_UNKNOWN = 0xFF,
} wifi_security_e;


typedef enum
{
    WIFI_SECURITY_CIPHER_NONE = 0,
    WIFI_SECURITY_CIPHER_WEP,
    WIFI_SECURITY_CIPHER_AES,
    WIFI_SECURITY_CIPHER_TKIP,
    WIFI_SECURITY_CIPHER_TKIP_AES,

}wifi_cipher_e;

// wifi error code
#define WIFI_ERROR_STA_AUTH_FAIL                              200
#define WIFI_ERROR_STA_ASSOC_FAIL                             201
#define WIFI_ERROR_STA_CONNECT_NO_TARGET_AP                   202
#define WIFI_ERROR_STA_LINK_LOSS                              203
#define WIFI_ERROR_WPA3_PWD_OR_AUTH_FAIL                      204
#define WIFI_ERROR_FOUND_SSID_BUT_KEY_MISMATCH                205
#define WIFI_ERROR_NO_FRAME_ALLC_FOR_AUTH_ASSO                206
#define WIFI_ERROR_ADD_STA_FAIL                               207
#define WIFI_ERROR_AUTH_ASSOC_TIMEOUT                         208
#define WIFI_ERROR_DEAUTH_BY_AP                               209
#define WIFI_ERROR_DEAUTH_BY_LOCAL                            210

// 1-99 is for spec definitions
#define WIFI_ERROR_UNSPECIFIED_REASON                         1
#define WIFI_ERROR_INVALID_AUTHENTICATION                     2
#define WIFI_ERROR_LEAVING_NETWORK_DEAUTH                     3
#define WIFI_ERROR_REASON_INACTIVITY                          4
#define WIFI_ERROR_NO_MORE_STAS                               5
#define WIFI_ERROR_INVALID_CLASS2_FRAME                       6
#define WIFI_ERROR_INVALID_CLASS3_FRAME                       7
#define WIFI_ERROR_LEAVING_NETWORK_DISASSOC                   8
#define WIFI_ERROR_NOT_AUTHENTICATED                          9
#define WIFI_ERROR_UNACCEPTABLE_POWER_CAPABILITY              10
#define WIFI_ERROR_UNACCEPTABLE_SUPPORTED_CHANNELS            11
#define WIFI_ERROR_BSS_TRANSITION_DISASSOC                    12
#define WIFI_ERROR_REASON_INVALID_ELEMENT                     13
#define WIFI_ERROR_MIC_FAILURE                                14
#define WIFI_ERROR_4WAY_HANDSHAKE_TIMEOUT                     15
#define WIFI_ERROR_GK_HANDSHAKE_TIMEOUT                       16
#define WIFI_ERROR_HANDSHAKE_ELEMENT_MISMATCH                 17
#define WIFI_ERROR_REASON_INVALID_GROUP_CIPHER                18
#define WIFI_ERROR_REASON_INVALID_PAIRWISE_CIPHER             19
#define WIFI_ERROR_REASON_INVALID_AKMP                        20
#define WIFI_ERROR_UNSUPPORTED_RSNE_VERSION                   21
#define WIFI_ERROR_INVALID_RSNE_CAPABILITIES                  22
#define WIFI_ERROR_802_1_X_AUTH_FAILED                        23
#define WIFI_ERROR_REASON_CIPHER_OUT_OF_POLICY                24
#define WIFI_ERROR_TDLS_TEARDOWN_PEER_UNREACHABLE             25
#define WIFI_ERROR_TDLS_TEARDOWN_UNSPECIFIED_REASON           26
#define WIFI_ERROR_SSP_REQUESTED_DISASSOC                     27
#define WIFI_ERROR_NO_SSP_ROAMING_AGREEMENT                   28
#define WIFI_ERROR_BAD_CIPHER_OR_AKM                          29
#define WIFI_ERROR_NOT_AUTHORIZED_THIS_LOCATION               30
#define WIFI_ERROR_SERVICE_CHANGE_PRECLUDES_TS                31
#define WIFI_ERROR_UNSPECIFIED_QOS_REASON                     32
#define WIFI_ERROR_NOT_ENOUGH_BANDWIDTH                       33
#define WIFI_ERROR_MISSING_ACKS                               34
#define WIFI_ERROR_EXCEEDED_TXOP                              35
#define WIFI_ERROR_STA_LEAVING                                36
#define WIFI_ERROR_END_TSEND_BA                               37
#define WIFI_ERROR_UNKNOWN_TSUNKNOWN_BA                       38
#define WIFI_ERROR_TIMEOUT                                    39
#define WIFI_ERROR_PEER_INITIATED                             46
#define WIFI_ERROR_AP_INITIATED                               47
#define WIFI_ERROR_REASON_INVALID_FT_ACTION_FRAME_COUNT       48
#define WIFI_ERROR_REASON_INVALID_PMKID                       49
#define WIFI_ERROR_REASON_INVALID_MDE                         50
#define WIFI_ERROR_REASON_INVALID_FTE                         51
#define WIFI_ERROR_POOR_RSSI_CONDITIONS                       71

/**************************/
/// wifi event id define
typedef enum
{
    EVENT_WIFI_INIT_DONE = 1,
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECT,
    EVENT_WIFI_GOT_IP,
    EVENT_WIFI_SCAN_DONE,
    EVENT_WIFI_AP_STARTED,
    EVENT_WIFI_AP_STOPPED,
    EVENT_WIFI_AP_STA_ADD,
    EVENT_WIFI_AP_STA_DEL,

    EVENT_WIFI_SNIFFER_STARTED,
    EVENT_WIFI_SNIFFER_STOPPED,

    // error event
    EVENT_WIFI_STA_CONNECT_FAIL,
    EVENT_WIFI_STA_DHCP_FAIL,

    EVENT_WIFI_MAX = 63,
} wifi_event_id_e;

// parameters for EVENT_WIFI_AP_STA_ADD report
typedef struct
{
    uint8_t sta_idx;  //peer sta_idx which is connected, use to get sta info by wifi_ap_get_sta_info
} event_ap_sta_add_param_t;

// parameters for EVENT_WIFI_AP_STA_DEL report,  use to get sta info by wifi_ap_get_sta_info
typedef struct
{
    uint8_t sta_idx;  //peer sta_idx which is disconnected
} event_ap_sta_del_param_t;

// parameters for EVENT_WIFI_CONNECT_FAIL report,  use to get connect fail reason code
typedef struct
{
    uint16_t erro_code;
    uint16_t status_code;   /* status in associate response*/
    uint16_t reason_code;   /* reason code in deauth */
    // it would be true when max retry count reach
    bool max_retry_reach;
} event_connect_fail_param_t;

// parameters for EVENT_WIFI_DISCONNECT report,  use to get disconnect reason code
typedef struct
{
    uint16_t erro_code;
    uint16_t status_code;        /* status in associate response*/
    uint16_t reason_code;   /* reason code in deauth */
    // it would be true when max retry count reach
    bool max_retry_reach;
} event_disconnect_param_t;

// parameters for EVENT_WIFI_SCAN_DONE report,  use to get scan status
typedef struct
{
    /// Status, 0 scan success, 1 fail
    uint32_t status;
    /// scan result available
    uint32_t result_cnt;
} event_scan_done_param_t;

/**************************/

#ifndef MAC_ADDR_LEN
/// MAC address length in bytes.
#define MAC_ADDR_LEN 6
#endif

#define MAX_CHANNELS_NUM (14)

/// SSID maximum length.
#define WIFI_SSID_LEN 32

/// password maximum length.
#define WIFI_PASSWORD_LEN 64

/// SSID.
typedef struct wifi_ssid
{
    /// Actual length of the SSID.
    uint8_t length;
    /// Array containing the SSID name.
    uint8_t array[WIFI_SSID_LEN + 1];
} wifi_ssid_t;

/// sta interface state
typedef enum
{
    STA_INACTIVE = 0,
    STA_IN_CONNECTING,
    STA_CONNECTED,
    STA_DISCONNECTED,

    STA_STATE_MAX = 255,
} wifi_sta_state_e;

/// wifi sta mode link status
typedef struct wifi_link_status
{
    /**
    * connected success or not. (Set to 0 if interface is not connected)
    */
    uint8_t is_connected;
    /**
    * sta state , refer wifi_sta_state_e
    */
    wifi_sta_state_e state;
    /**
     * BSSID of the AP. (Set to 0 if interface is not connected)
     */
    uint8_t bssid[MAC_ADDR_LEN];
    /**
     * RSSI (in dBm) of the last received beacon. (valid only if connected)
     */
    int8_t rssi;
    /**
     * noise (in dBm), detected in the last beacon duration. (valid only if connected)
     */
    int8_t noise;
    /**
     * SSID of the AP (valid only if connected)
     */
    char ssid[WIFI_SSID_LEN + 1];
    /**
     * channel number of the AP (valid only if connected)
     */
    int8_t channel;
    /**
     * AID of the STA (valid only if connected)
     */
    uint16_t aid;
} wifi_link_status_t;

typedef struct wifi_sniffer_frame_info
{
    /**
     * Interface index that received the frame. (-1 if unknown)
     */
    int fvif_idx;
    /**
     * Length (in bytes) of the frame.
     */
    uint16_t length;
    /**
     * Primary channel frequency (in MHz) on which the frame has been received.
     */
    uint16_t freq;
    /**
     * Received signal strength (in dBm)
     */
    int16_t rssi;
    /**
     * Received signal strength (in dBm)
     */
    int16_t rssi_secagc;
    /**
     * Received SNR
     */
    uint8_t snr;
    /**
     * Received SNR
     */
    uint8_t snr_secagc;
    /**
     * Received NOISE
     */
    uint8_t noise;
    /**
     * Received NOISE
     */
    uint8_t noise_secagc;
    /**
     * Frame payload. Can be NULL if sniffer mode is started with @p uf parameter set to
     * true. In this case all other fields are still valid.
     */
    uint8_t *payload;
} wifi_sniffer_frame_info_t;

typedef void (*wifi_sniffer_rx_cb)(wifi_sniffer_frame_info_t *info, void *cb_arg);

typedef struct sniffer_para
{
    /// interface index
    uint8_t itf_idx;
    /// Channel bandwidth (@ref wifi_bandwidth_e)
    uint8_t bw;
    /// Frequency for Primary 20MHz channel (in MHz)
    uint16_t prim20_freq;
    /// Frequency center of the contiguous channel or center of Primary 80+80 (in MHz)
    uint16_t center1_freq;
    /// Frequency center of the contiguous channel of Secondary 80+80 (in MHz)
    uint16_t center2_freq;
    /// Frame received callback.
    wifi_sniffer_rx_cb cb;
    /// Parameter for the sniffer callback
    void *cb_arg;
} wifi_sniffer_para_t;

typedef enum
{
    DHCP_CLIENT = 0,
    //use static ipv4 address
    STATIC_IPV4,
} wifi_dhcp_mode_e;

typedef enum
{
    /// full channel scan
    NORMAL_SCAN = 0,
    /// scan would stop if ssid matched and rssi > rssi threshold
    FAST_SCAN,
} wifi_scan_method_e;

typedef struct wifi_connect_cfg
{
    /**
     * SSID to connect to (mandatory)
     */
    uint8_t ssid[WIFI_SSID_LEN + 1];
    /**
     * dhcp mode (mandatory)
     */
    wifi_dhcp_mode_e dhcp_mode;
    /**
     * AP password/PSK passed as a string (i.e. MUST be terminated by a null byte)
     */
    uint8_t key[WIFI_PASSWORD_LEN + 1];
    /**
     * AP BSSID. Optional, clear it to 0 if not set.
     */
    uint8_t bssid[MAC_ADDR_LEN];
    /**
     * AP frequency. Optional, you can specify up to two frequencies on which AP
     * will be scanned. This is to speed up connection and should be set 0 if no used.
     */
    uint16_t freq[2];
    /**
     * security mode, suggest to choose auto (WIFI_SEC_AUTO)
     */
    wifi_security_e sec;
    /**
     * config static ip when set dhcp_mode to STATIC_IPV4
     * not use when set dhcp_mode to DHCP_CLIENT
     */
    char ip4_ip[16];    //e.g. "192.168.1.100"
    char ip4_mask[16];  //e.g. "255.255.255.0"
    char ip4_gw[16];    //e.g. "192.168.1.1"
    /**
     * retry count setting of connect failure
     */
    uint8_t failure_retry_cnt;
    /**
    * support 1. normal scan full channel
    *         2. fast scan: scan would stop if specified ssid was found and rssi > rssi threshold
    * ref wifi_scan_method_e
    */
    wifi_scan_method_e scan_method;
    /**
    * rssi threshold setting for fast scan, optional
    */
    int8_t rssi_threshold;
} wifi_connect_cfg_t;


/// scan params
typedef struct wifi_scan_params
{
    uint8_t ssid_len;
    uint8_t ssid_array[WIFI_SSID_LEN];
    uint8_t bssid[MAC_ADDR_LEN];
    uint8_t bssid_set_flag;
    uint8_t channel_cnt;
    /**
     * channel 1~ 14
     * channel[0] = 10  scan channel 10
    */
    uint8_t channel[MAX_CHANNELS_NUM];
    /**
    * support 1. normal scan full channel
    *         2. fast scan: scan would stop if specified ssid was found and rssi > rssi threshold
    * ref wifi_scan_method_e
    */
    wifi_scan_method_e scan_method;
    /**
    * rssi threshold setting for fast scan, optional
    */
    int8_t rssi_threshold;
    /**
    * Scan duration, in ms
    * max duration 150ms
    */
    int32_t duration;
} wifi_scan_params_t;

/// bss mode b/g/n/ax
typedef enum
{
    /// 802.ll b
    WIFI_MODE_802_11B       = 0x01,
    /// 802.11 a
    WIFI_MODE_802_11A       = 0x02,
    /// 802.11 g
    WIFI_MODE_802_11G       = 0x04,
    /// 802.11n at 2.4GHz
    WIFI_MODE_802_11N_2_4   = 0x08,
    /// 802.11n at 5GHz
    WIFI_MODE_802_11N_5     = 0x10,
    /// 802.11ac at 5GHz
    WIFI_MODE_802_11AC_5    = 0x20,
    /// 802.11ax at 2.4GHz
    WIFI_MODE_802_11AX_2_4  = 0x40,
    /// 802.11ax at 5GHz
    WIFI_MODE_802_11AX_5    = 0x80,
} wifi_bss_mode_e;

typedef struct wifi_scan_result
{
    /// wifi bss mode bgn/ax
    uint32_t mode;
    uint32_t last_timestamp;
    uint8_t ssid_len;
    char ssid[WIFI_SSID_LEN + 1];
    uint8_t bssid[MAC_ADDR_LEN];
    uint8_t channel;
    int8_t rssi;
    uint8_t auth;
    uint8_t cipher;
    uint8_t gtk_cipher;
    uint8_t wps;
    uint8_t is_used;
} wifi_scan_result_t;

typedef struct wifi_sta_basic_info
{
    uint8_t  sta_idx;
    uint8_t  is_used;
    uint8_t  sta_mac[MAC_ADDR_LEN];
    uint16_t aid;
  //  char     ip[16]; //e.g. "192.168.1.100"
} wifi_sta_basic_info_t;

/// ap start params
typedef struct wifi_ap_cfg_params
{
    /// must be set
    uint8_t ssid[WIFI_SSID_LEN + 1];;
    /// AP password/PSK passed as a string (i.e. MUST be terminated by a null byte)
    uint8_t pwd[WIFI_PASSWORD_LEN + 1];;
    /// OPEN/WPA/WPA2 can be supported
    /// if WIFI_SECURITY_AUTO is setted, will be OPEN mode when pwd is NULL or WPA2 mode when pwd exists
    wifi_security_e sec;
    /// if zero, the default value 6 will be setted
    uint8_t channel;
    /// Channel bandwidth (@ref wifi_bandwidth_e)
    uint8_t bw;
    ///ap_max_inactivity - Timeout in seconds to detect STA's inactivity
    ///This timeout value is used in AP mode to clean up inactive stations.
    uint32_t ap_max_inactivity;
} wifi_ap_cfg_params_t;

typedef struct wifi_connect_ind_stat_info
{
    uint16_t status_code;
    uint16_t reason_code;
    char ssid[WIFI_SSID_LEN + 1];
    char pwd[WIFI_PASSWORD_LEN + 1];
    /// BSSID
    uint8_t bssid[MAC_ADDR_LEN];
    /// band 2.4G or 5G
    uint8_t chan_band;
    /// channel
    uint8_t channel;
    /// Association Id allocated by the AP for this connection
    uint16_t aid;
    /// Index of the VIF for which the association process is complete
    uint8_t vif_idx;
    /// Index of the STA entry allocated for the AP
    uint8_t ap_sta_idx;
    /// Index of the LMAC channel context the connection is attached to
    uint8_t ch_idx;
    /// Flag indicating if the AP is supporting QoS
    bool qos;
    /// Flags indicating which BSS capabilities are valid (HT/VHT/QoS/HE/etc.)
    uint32_t capa_flags;
} wifi_connect_ind_stat_info_t;

typedef struct wifi_conf
{
    char country_code[3];
    int channel_nums;
} wifi_conf_t;

typedef struct wifi_ap_info
{
    uint8_t ssid[WIFI_SSID_LEN + 1];         /**< SSID of the Wi-Fi network. */
    uint8_t bssid[MAC_ADDR_LEN];         /**< BSSID of the Wi-Fi network. */
    uint8_t security;         /**< Wi-Fi Security. */
    uint8_t pwd[WIFI_PASSWORD_LEN + 1];          /**< WPA/WPA2 passphrase. */
    uint8_t channel;          /**< Channel number. */
} wifi_ap_info_t;

/**
 * @brief Wi-Fi 80211 frame TX control.
 */
typedef struct wifi_80211_tx_info
{
    uint8_t* pkt;                   /**< 80211 data tx packet address */
    int      len;                   /**< 80211 data tx packet length */
    uint8_t  wifi_vif_idx;          /**refer to wifi_vif_idx_e, indicate wifi mode index*/
} wifi_80211_tx_info_t;

/**
  * @brief WiFi PHY rate encodings
  *
  */
typedef enum
{
    WIFI_PHY_RATE_1M_L      = 0x00, /**< 1 Mbps with long preamble */
    WIFI_PHY_RATE_2M_L      = 0x01, /**< 2 Mbps with long preamble */
    WIFI_PHY_RATE_5M_L      = 0x02, /**< 5.5 Mbps with long preamble */
    WIFI_PHY_RATE_11M_L     = 0x03, /**< 11 Mbps with long preamble */
    WIFI_PHY_RATE_2M_S      = 0x05, /**< 2 Mbps with short preamble */
    WIFI_PHY_RATE_5M_S      = 0x06, /**< 5.5 Mbps with short preamble */
    WIFI_PHY_RATE_11M_S     = 0x07, /**< 11 Mbps with short preamble */

    WIFI_PHY_RATE_6M        = 0x10, /**< 6 Mbps */
    WIFI_PHY_RATE_9M        = 0x11, /**< 9 Mbps */
    WIFI_PHY_RATE_12M       = 0x12, /**< 12 Mbps */
    WIFI_PHY_RATE_18M       = 0x13, /**< 18 Mbps */
    WIFI_PHY_RATE_24M       = 0x14, /**< 24 Mbps */
    WIFI_PHY_RATE_36M       = 0x15, /**< 36 Mbps */
    WIFI_PHY_RATE_48M       = 0x16, /**< 48 Mbps */
    WIFI_PHY_RATE_54M       = 0x17, /**< 54 Mbps */
    /**< rate table and guard interval information for each MCS rate*/
    /*
     -----------------------------------------------------------------------------------------------------------
            MCS RATE             |          HT20           |          HT40           |          HE20           |
     WIFI_PHY_RATE_MCS0_LGI      |     6.5 Mbps (800ns)    |    13.5 Mbps (800ns)    |     8.1 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS1_LGI      |      13 Mbps (800ns)    |      27 Mbps (800ns)    |    16.3 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS2_LGI      |    19.5 Mbps (800ns)    |    40.5 Mbps (800ns)    |    24.4 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS3_LGI      |      26 Mbps (800ns)    |      54 Mbps (800ns)    |    32.5 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS4_LGI      |      39 Mbps (800ns)    |      81 Mbps (800ns)    |    48.8 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS5_LGI      |      52 Mbps (800ns)    |     108 Mbps (800ns)    |      65 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS6_LGI      |    58.5 Mbps (800ns)    |   121.5 Mbps (800ns)    |    73.1 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS7_LGI      |      65 Mbps (800ns)    |     135 Mbps (800ns)    |    81.3 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS8_LGI      |          -----          |          -----          |    97.5 Mbps (1600ns)   |
     WIFI_PHY_RATE_MCS9_LGI      |          -----          |          -----          |   108.3 Mbps (1600ns)   |
     -----------------------------------------------------------------------------------------------------------
    */
    WIFI_PHY_RATE_MCS0_LGI  = 0x20, /**< MCS0 with long GI */
    WIFI_PHY_RATE_MCS1_LGI  = 0x21, /**< MCS1 with long GI */
    WIFI_PHY_RATE_MCS2_LGI  = 0x22, /**< MCS2 with long GI */
    WIFI_PHY_RATE_MCS3_LGI  = 0x23, /**< MCS3 with long GI */
    WIFI_PHY_RATE_MCS4_LGI  = 0x24, /**< MCS4 with long GI */
    WIFI_PHY_RATE_MCS5_LGI  = 0x25, /**< MCS5 with long GI */
    WIFI_PHY_RATE_MCS6_LGI  = 0x26, /**< MCS6 with long GI */
    WIFI_PHY_RATE_MCS7_LGI  = 0x27, /**< MCS7 with long GI */
    WIFI_PHY_RATE_MCS8_LGI  = 0x28, /**< MCS8 with long GI for WIFI6 */
    WIFI_PHY_RATE_MCS9_LGI  = 0x29, /**< MCS9 with long GI for WIFI6 */
    /*
     -----------------------------------------------------------------------------------------------------------
            MCS RATE             |          HT20           |          HT40           |          HE20           |
     WIFI_PHY_RATE_MCS0_SGI      |     7.2 Mbps (400ns)    |      15 Mbps (400ns)    |      8.6 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS1_SGI      |    14.4 Mbps (400ns)    |      30 Mbps (400ns)    |     17.2 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS2_SGI      |    21.7 Mbps (400ns)    |      45 Mbps (400ns)    |     25.8 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS3_SGI      |    28.9 Mbps (400ns)    |      60 Mbps (400ns)    |     34.4 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS4_SGI      |    43.3 Mbps (400ns)    |      90 Mbps (400ns)    |     51.6 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS5_SGI      |    57.8 Mbps (400ns)    |     120 Mbps (400ns)    |     68.8 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS6_SGI      |      65 Mbps (400ns)    |     135 Mbps (400ns)    |     77.4 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS7_SGI      |    72.2 Mbps (400ns)    |     150 Mbps (400ns)    |       86 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS8_SGI      |          -----          |          -----          |    103.2 Mbps (800ns)   |
     WIFI_PHY_RATE_MCS9_SGI      |          -----          |          -----          |    114.7 Mbps (800ns)   |
     -----------------------------------------------------------------------------------------------------------
    */
    WIFI_PHY_RATE_MCS0_SGI  = 0x30, /**< MCS0 with short GI */
    WIFI_PHY_RATE_MCS1_SGI  = 0x31, /**< MCS1 with short GI */
    WIFI_PHY_RATE_MCS2_SGI  = 0x32, /**< MCS2 with short GI */
    WIFI_PHY_RATE_MCS3_SGI  = 0x33, /**< MCS3 with short GI */
    WIFI_PHY_RATE_MCS4_SGI  = 0x34, /**< MCS4 with short GI */
    WIFI_PHY_RATE_MCS5_SGI  = 0x35, /**< MCS5 with short GI */
    WIFI_PHY_RATE_MCS6_SGI  = 0x36, /**< MCS6 with short GI */
    WIFI_PHY_RATE_MCS7_SGI  = 0x37, /**< MCS7 with short GI */
    WIFI_PHY_RATE_MCS8_SGI  = 0x38, /**< MCS8 with short GI for WIFI6*/
    WIFI_PHY_RATE_MCS9_SGI  = 0x39, /**< MCS9 with short GI for WIFI6*/
    /*
     -----------------------------------------------------------------------------------------------------------
       WIFI6 3.2us MCS RATE         |               HE20            |
     WIFI_PHY_RATE_MCS0_3D2SGI      |           7.3 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS1_3D2SGI      |          14.6 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS2_3D2SGI      |          21.9 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS3_3D2SGI      |          29.3 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS4_3D2SGI      |          43.9 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS5_3D2SGI      |          58.5 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS6_3D2SGI      |          65.8 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS7_3D2SGI      |          73.1 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS8_3D2SGI      |          87.8 Mbps (3200ns)   |
     WIFI_PHY_RATE_MCS9_3D2SGI      |          97.5 Mbps (3200ns)   |
     -----------------------------------------------------------------------------------------------------------
    */

    WIFI_PHY_RATE_MCS0_3D2GI  = 0x40, /**< MCS0 with short GI */
    WIFI_PHY_RATE_MCS1_3D2GI  = 0x41, /**< MCS1 with short GI */
    WIFI_PHY_RATE_MCS2_3D2GI  = 0x42, /**< MCS2 with short GI */
    WIFI_PHY_RATE_MCS3_3D2GI  = 0x43, /**< MCS3 with short GI */
    WIFI_PHY_RATE_MCS4_3D2GI  = 0x44, /**< MCS4 with short GI */
    WIFI_PHY_RATE_MCS5_3D2GI  = 0x45, /**< MCS5 with short GI */
    WIFI_PHY_RATE_MCS6_3D2GI  = 0x46, /**< MCS6 with short GI */
    WIFI_PHY_RATE_MCS7_3D2GI  = 0x47, /**< MCS7 with short GI */
    WIFI_PHY_RATE_MCS8_3D2GI  = 0x48, /**< MCS8 with short GI */
    WIFI_PHY_RATE_MCS9_3D2GI  = 0x49, /**< MCS9 with short GI */

    WIFI_PHY_RATE_AUTO = 0xff,               /** auto rate */
    WIFI_PHY_RATE_MAX,
} wifi_phy_rate_e;

typedef struct pwr_table
{
    int8_t     pwr_11b[4];
    int8_t     pwr_11g[8];
    int8_t     pwr_11n_ht20[8];
    int8_t     pwr_11ax_he20[10];
#ifdef CFG_5G
    int8_t     pwr_11ac_vht20[10];
    int8_t     pwr_11n_ht40[8];
    int8_t     pwr_11ac_vht40[10];
    int8_t     pwr_11ax_he40[10];
#endif
} pwr_table_t;

typedef enum
{
    CHAN_ALL = 0,  /** all channel type */
    LOW_CHAN = 1,  /** low channel type */
    MID_CHAN = 2,  /** mid channel type */
    HIGH_CHAN = 3  /** high channel type */
} chan_type_e;


struct wifi_mac_hdr
{
    uint16_t fctl;
    uint16_t durid;
    uint8_t addr1[MAC_ADDR_LEN];
    uint8_t addr2[MAC_ADDR_LEN];
    uint8_t addr3[MAC_ADDR_LEN];
    uint16_t seq;
} __PACKED;


typedef const struct wifi_ops {
    uint8_t dpd_track_temp_disable;
    uint8_t dpd_track_connect_en;
    uint8_t fw_log_level; /* 1~5 CRT/ERR/WAR/INFO/VRB, default level 4*/
    int8_t (* get_mac)(uint8_t *mac_addr);
    bool (* temp_update)(void);

} _wifi_ops, *pwifi_ops;

#define WIFI_PS_LOCK_BIT_APP           0x00000001
#define WIFI_PS_LOCK_BIT_FHOST_CNTRL   0x00000002
#define WIFI_PS_LOCK_BIT_FHOST_RX      0x00000004
#define WIFI_PS_LOCK_BIT_LWIP          0x00000008

typedef enum {
    WIFI_PS_MODE_OFF,        /**< WiFi power save disabled */
    WIFI_PS_MODE_DTIM,       /**< Wake on every DTIM beacon interval */
    WIFI_PS_MODE_LISTEN,   /**< Wake on each configured listen interval */
    WIFI_PS_MODE_MAX
} wifi_ps_mode_e;

#define WIFI_PS_DEFAULT_TYPE           WIFI_PS_MODE_DTIM

typedef struct wifi_ps_state
{
    bool state;
    bool enable;
    bool event_pending;
    uint32_t lock_state;
    uint32_t prevent;
    uint32_t vif_prevent;
    uint32_t tx_cnt;
    uint32_t timer_prevent;
} wifi_ps_state_t;

#endif
