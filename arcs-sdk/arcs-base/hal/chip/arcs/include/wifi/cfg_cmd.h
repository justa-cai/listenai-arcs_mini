/**
 ****************************************************************************************
 *
 * @file cfg_cmd.h
 *
 *
 * Copyright (C) ListenAI 2023
 *
 ****************************************************************************************
 */
#ifndef _CFG_CMD_H_
#define _CFG_CMD_H_
#if defined(NX_SUPPORT_AMP_IPC) || defined(CFG_AMP_IPC)
#include "ipc_shared.h"
#include "ls_event.h"
#endif
/// CFGRWNX index message
enum cfgrwnx_msg_index
{
    /// Sent by supplicant to retrieve HW capability (param: none)
    CFGRWNX_HW_FEATURE_CMD = 1,
    /// Response to CFGRWNX_HW_FEATURE_CMD (param: @ref cfgrwnx_hw_feature)
    CFGRWNX_HW_FEATURE_RESP,
    /// Sent by supplicant to retrieve FW capability (param: none)
    CFGRWNX_GET_CAPA_CMD,
    /// Response to CFGRWNX_GET_CAPA_CMD (param: none)
    CFGRWNX_GET_CAPA_RESP,
    /// Sent by Supplicant to install/remove Encryption key (param: @ref cfgrwnx_set_key)
    CFGRWNX_SET_KEY_CMD,
    /// Response to CFGRWNX_SET_KEY_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SET_KEY_RESP,
    /// Sent by Supplicant to start a SCAN (param: @ref cfgrwnx_scan)
    CFGRWNX_SCAN_CMD,
    /// Response to CFGRWNX_SCAN_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SCAN_RESP,
    /// Event sent when query scanu stat (param: @ref cfgrwnx_scanu_stat)
    CFGRWNX_API_SCANU_STAT,
    /// Event sent when query scanu stat (param: @ref cfgrwnx_scanu_stat_resp)
    CFGRWNX_API_SCANU_STAT_RESP,
    /// Event sent when scan is done (param: @ref cfgrwnx_scan_completed)
    CFGRWNX_SCAN_DONE_EVENT,
    /// Event sent when a new AP is found (param: @ref cfgrwnx_scan_result)
    CFGRWNX_SCAN_RESULT_EVENT,
    /// Sent by supplicant to initiate a connection (param: @ref cfgrwnx_connect)
    CFGRWNX_CONNECT_CMD,
    /// Response to CFGRWNX_CONNECT_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_CONNECT_RESP,
    /// Event sent when the connection is finished (param: @ref cfgrwnx_connect_event)
    CFGRWNX_CONNECT_EVENT,
    /// Sent by supplicant to end a connection (param: @ref cfgrwnx_disconnect)
    CFGRWNX_DISCONNECT_CMD,
    /// Response to CFGRWNX_DISCONNECT_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_DISCONNECT_RESP,
    /// Event sent if the connection is lost (param: @ref cfgrwnx_disconnect_event)
    CFGRWNX_DISCONNECT_EVENT,
    /// Sent by supplicant to open/close a control port (param: @ref cfgrwnx_ctrl_port)
    CFGRWNX_CTRL_PORT_CMD,
    /// Response to CFGRWNX_CTRL_PORT_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_CTRL_PORT_RESP,
    /// Event sent if a Michael MIC failure is detected (param: @ref
    /// cfgrwnx_mic_failure_event)
    CFGRWNX_MIC_FAILURE_EVENT,
    /// Sent by Application to retrieve system statistics (param: none)
    CFGRWNX_SYS_STATS_CMD,
    /// Response to CFGRWNX_SYS_STATS_CMD (param: @ref cfgrwnx_sys_stats_resp)
    CFGRWNX_SYS_STATS_RESP,
    /// Sent by smartconfig to obtain scan results (param: none)
    CFGRWNX_SCAN_RESULTS_CMD,
    /// Response to CFGRWNX_SCAN_RESULTS_CMD (param: @ref cfgrwnx_scan_results_resp)
    CFGRWNX_SCAN_RESULTS_RESP,
    /// Sent by Application to retrieve FW/PHY supported features (param: none)
    CFGRWNX_LIST_FEATURES_CMD,
    /// Response to CFGRWNX_LIST_FEATURES_CMD (param: @ref cfgrwnx_list_features_resp)
    CFGRWNX_LIST_FEATURES_RESP,
    /// Sent to change the type of an vif at MAC level. MAC VIF is deleted (if it exists)
    /// and re-created with the new type (unless type is VIF_UNKNOWN) (param: @ref
    /// cfgrwnx_set_vif_type)
    CFGRWNX_SET_VIF_TYPE_CMD,
    /// Response to CFGRWNX_SET_VIF_TYPE_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SET_VIF_TYPE_RESP,
    /// Sent by Application to configure a monitor interface (param: @ref
    /// cfgrwnx_monitor_cfg)
    CFGRWNX_MONITOR_CFG_CMD,
    /// Response to CFGRWNX_MONITOR_CFG_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_MONITOR_CFG_RESP,
    /// Event sent by the RX task when management frame is forwarded by the wifi task
    /// (param: @ref cfgrwnx_rx_mgmt_event)
    CFGRWNX_RX_MGMT_EVENT,
    /// Event sent by the RX task when spurious frame (i.e. clas 2/3 frame sent by
    /// unknown sta) is received. (param: @ref cfgrwnx_rx_spurious_event)
    CFGRWNX_RX_SPURIOUS_EVENT,
    /// Event to defer TX status processing (param: @ref cfgrwnx_tx_status_event)
    CFGRWNX_TX_STATUS_EVENT,
    /// Event sent by wifi task to request external authentication (i.e. Supplicant will
    /// do the authentication procedure for the wifi task, used for SAE) (param: @ref
    /// cfgrwnx_external_auth_event)
    CFGRWNX_EXTERNAL_AUTH_EVENT,
    /// Sent by Supplicant to pass external authentication status (param: @ref
    /// cfgrwnx_external_auth_status)
    CFGRWNX_EXTERNAL_AUTH_STATUS_RESP,
    /// Sent by Supplicant to start an AP (param: @ref cfgrwnx_start_ap)
    CFGRWNX_START_AP_CMD,
    /// Response to CFGRWNX_START_AP_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_START_AP_RESP,
    /// Sent by Supplicant to stop an AP (param: @ref cfgrwnx_stop_ap)
    CFGRWNX_STOP_AP_CMD,
    /// Response to CFGRWNX_STOP_AP_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_STOP_AP_RESP,
    /// Sent by Supplicant to configure EDCA parameter for one AC (param: @ref
    /// cfgrwnx_set_edca)
    CFGRWNX_SET_EDCA_CMD,
    /// Response to CFGRWNX_SET_EDCA_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SET_EDCA_RESP,
    /// Sent by Supplicant to update the beacon (param: @ref cfgrwnx_bcn_update)
    CFGRWNX_BCN_UPDATE_CMD,
    /// Response to CFGRWNX_BCN_UPDATE (param: @ref cfgrwnx_resp)
    CFGRWNX_BCN_UPDATE_RESP,
    /// Send by supplicant to register a new Station (param: @ref cfgrwnx_sta_add)
    CFGRWNX_STA_ADD_CMD,
    /// Response to CFGRWNX_STA_ADD_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_STA_ADD_RESP,
    /// Send by supplicant to un-register a Station (param: @ref cfgrwnx_sta_remove)
    CFGRWNX_STA_REMOVE_CMD,
    /// Response to CFGRWNX_STA_REMOVE_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_STA_REMOVE_RESP,
    /// Send by supplicant to retrieve Key current sequence number (param: @ref
    /// cfgrwnx_key_seqnum)
    CFGRWNX_KEY_SEQNUM_CMD,
    /// Response to CFGRWNX_KEY_SEQNUM_CMD (param: @ref cfgrwnx_key_seqnum_resp)
    CFGRWNX_KEY_SEQNUM_RESP,
    /// Enable Power Save (param: @ref cfgrwnx_set_ps_mode)
    CFGRWNX_SET_PS_MODE_CMD,
    /// Response to CFGRWNX_SET_PS_MODE_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SET_PS_MODE_RESP,
    /// Request statistics information for a station (param: @ref cfgrwnx_get_sta_info)
    CFGRWNX_GET_STA_INFO_CMD,
    /// Response to CFGRWNX_GET_STA_INFO_CMD with statistics of the station
    /// (param: @ref cfgrwnx_get_sta_info_resp)
    CFGRWNX_GET_STA_INFO_RESP,
    /// Request to probe if a client is still present (param: @ref cfgrwnx_probe_client)
    CFGRWNX_PROBE_CLIENT_CMD,
    /// Response to CFGRWNX_PROBE_CLIENT_CMD. Only indicates if request was valid or not.
    /// The actual status will be in CFGRWNX_PROBE_CLIENT_EVENT (param: @ref cfgrwnx_resp)
    CFGRWNX_PROBE_CLIENT_RESP,
    /// Event sent after receiving CFGRWNX_PROBE_CLIENT_CMD, to indicate the actual client
    /// status (param: @ref cfgrwnx_probe_client_event)
    CFGRWNX_PROBE_CLIENT_EVENT,
    /// Request to remain on specific channel (param: @ref cfgrwnx_remain_on_channel)
    CFGRWNX_REMAIN_ON_CHANNEL_CMD,
    /// Response to CFGRWNX_REMAIN_ON_CHANNEL_CMD. Only indicates if request was valid or
    /// not. The actual status will be in CFGRWNX_REMAIN_ON_CHANNEL_EVENT (param: @ref
    /// cfgrwnx_resp)
    CFGRWNX_REMAIN_ON_CHANNEL_RESP,
    /// Event sent after receiving CFGRWNX_REMAIN_ON_CHANNEL_CMD, to indicate that the
    /// procedure is completed (param: @ref cfgrwnx_remain_on_channel_event)
    CFGRWNX_REMAIN_ON_CHANNEL_EVENT,
    /// Request to cancel remain on channel (param: @ref cfgrwnx_cancel_remain_on_channel)
    CFGRWNX_CANCEL_REMAIN_ON_CHANNEL_CMD,
    /// Response to CFGRWNX_CANCEL_REMAIN_ON_CHANNEL_CMD. Only indicates if request was
    /// valid or not. The actual status will be in CFGRWNX_CANCEL_REMAIN_ON_CHANNEL_EVENT
    /// (param: @ref cfgrwnx_resp)
    CFGRWNX_CANCEL_REMAIN_ON_CHANNEL_RESP,
    /// Event sent after receiving CFGRWNX_CANCEL_REMAIN_ON_CHANNEL_CMD, to indicate that
    /// the procedure is completed (param: @ref cfgrwnx_remain_on_channel_event)
    CFGRWNX_REMAIN_ON_CHANNEL_EXP_EVENT,
    /// Request RC statistics for a station (param: @ref cfgrwnx_rc)
    CFGRWNX_RC_CMD,
    /// Response to CFGRWNX_RC_CMD with the RC statisticts (param: @ref cfgrwnx_rc_result)
    CFGRWNX_RC_RESP,
    /// Request by Application to setup NOA protocol (param: @ref cfgrwnx_p2p_noa_cmd)
    CFGRWNX_P2P_NOA_CMD,
    /// Response to CFGRWNX_P2P_NOA_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_P2P_NOA_RESP,
    /// Request to set RC rate (param: @ref cfgrwnx_rc_set_rate)
    CFGRWNX_RC_SET_RATE_CMD,
    /// Response to CFGRWNX_RC_SET_RATE_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_RC_SET_RATE_RESP,
    /// Request to join a mesh network (param: @ref cfgrwnx_join_mesh_cmd)
    CFGRWNX_JOIN_MESH_CMD,
    /// Response to CFGRWNX_JOIN_MESH_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_JOIN_MESH_RESP,
    /// Request to leave a mesh network (param: @ref cfgrwnx_leave_mesh_cmd)
    CFGRWNX_LEAVE_MESH_CMD,
    /// Response to CFGRWNX_LEAVE_MESH_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_LEAVE_MESH_RESP,
    /// Notification that a connection has been established or lost (param: @ref
    /// cfgrwnx_mesh_peer_update_ntf)
    CFGRWNX_MESH_PEER_UPDATE_NTF_CMD,
    /// Response to CFGRWNX_MESH_PEER_UPDATE_NTF_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_MESH_PEER_UPDATE_NTF_RESP,
    /// Notification of a new peer candidate MESH (param: @ref
    /// cfgrwnx_new_peer_candidate_event)
    CFGRWNX_NEW_PEER_CANDIDATE_EVENT,
    /// Request by Application to setup a FTM measurement (param: @ref
    /// cfgrwnx_ftm_start_cmd)
    CFGRWNX_FTM_START_CMD,
    /// Response to CFGRWNX_FTM_START_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_FTM_START_RESP,
    /// Event sent after receiving CFGRWNX_FTM_START_CMD, to indicate that the
    /// procedure is completed (param: @ref cfgrwnx_ftm_done_event)
    CFGRWNX_FTM_DONE_EVENT,
    /// Request update of RX filter set in MACHW (param: @ref cfgrwnx_rx_filter)
    CFGRWNX_RX_FILTER_SET_CMD,
    /// Response to CFGRWNX_RX_FILTER_SET_CMD (param: none)
    CFGRWNX_RX_FILTER_SET_RESP,
    /// Request to start/update an individual TWT agreement
    /// (param: @ref cfgrwnx_twt_setup_cmd)
    CFGRWNX_TWT_SETUP_CMD,
    /// Response to CFGRWNX_TWT_SETUP_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_TWT_SETUP_RESP,
    /// Request to end an individual TWT agreement (param: @ref cfgrwnx_twt_teardown_cmd)
    CFGRWNX_TWT_TEARDOWN_CMD,
    /// Response to CFGRWNX_TWT_TEARDOWN_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_TWT_TEARDOWN_RESP,
    /// Event sent after receiving response to a TWT S1G action frame (setup or teardown)
    /// (param: @ref cfgrwnx_twt_event)
    CFGRWNX_TWT_EVENT,
    /// Event sent when unprotected management frame is received in a SA with PMF enabled.
    /// (param: @ref cfgrwnx_rx_unprot_mgmt_event)
    CFGRWNX_RX_UNPROT_MGMT_EVENT,
    /// Add a new interface (param: @ref cfgrwnx_if_add)
    CFGRWNX_IF_ADD_CMD,
    /// Response to CFGRWNX_IF_ADD_CMD (param: @ref cfgrwnx_if_add_resp)
    CFGRWNX_IF_ADD_RESP,
    /// Remove a new interface (param: @ref cfgrwnx_if_remove)
    CFGRWNX_IF_REMOVE_CMD,
    /// Response to CFGRWNX_IF_REMOVE_CMD (param: @ref cfgrwnx_if_remove_resp)
    CFGRWNX_IF_REMOVE_RESP,
    /// Set IP address
    CFGRWNX_SET_IP_CMD,
    /// CLI command (param: @ref cfgrwnx_cli_cmd)
    CFGRWNX_CLI_CMD,
    /// IPC event for connection (param: @ref cfgrwnx_connect_ipc_event)
    CFGRWNX_CONNECT_IPC_EVENT,
    /// Control port setting
    CFGRWNX_SET_CONTROL_PORT_EVENT,
    /// Send data frame
    CFGRWNX_TX_DATA_CMD,
    /// Reveive data frame
    CFGRWNX_RX_DATA_EVENT,
    /// RSSI indication
    CFGRWNX_RSSI_STATUS_EVENT,
    /// TX frame credits updating indication
    CFGRWNX_TX_CREDITS_UPDATE_EVENT,
    /// Command credits updating indication from FW to host
    CFGRWNX_CMD_TX_CREDITS_UPDATE_EVENT,
    /// Scan result indication
    //CFGRWNX_SCAN_RESULT_IND,
    /// Reset FW
    CFGRWNX_RESET_FW_CMD,
    /// Set passphrase
    CFGRWNX_SET_PASSPHRASE_CMD,
    /// Set MAC address for the interfaces
    CFGRWNX_SET_MAC_CMD,
    /// Set config
    CFGRWNX_CONFIG_CMD,
    CFGRWNX_CONFIG_RESP,
    /// Power on/off WiFi
    CFGRWNX_POWER_ON_CMD,
    /// GOT IP CMD
    CFGRWNX_GOT_IP_CMD,
    /// GOT IP RESP
    CFGRWNX_GOT_IP_RESP,
    /// Set dbg log level (param: @ref cfgrwnx_set_sev_filter)
    CFGRWNX_SET_SEV_FILTER_CMD,
    /// Response to CFGRWNX_SET_SEV_FILTER_CMD (param: @ref cfgrwnx_if_add_resp)
    CFGRWNX_SET_SEV_FILTER_RESP,
    CFGRWNX_IPC_ECHO_REQ_CMD,
    CFGRWNX_IPC_ECHO_REPLY_CMD,
    /// Set MAC idle (param: @ref cfgrwnx_set_mac_idle)
    CFGRWNX_SET_MAC_IDLE_CMD,
    /// Response to CFGRWNX_SET_MAC_IDLE_CMD (param: @ref cfgrwnx_resp)
    CFGRWNX_SET_MAC_IDLE_RESP,
};

/// CFGRWNX status
enum cfgrwnx_status
{
    /// Success status
    CFGRWNX_SUCCESS = 0,
    /// Generic error status
    CFGRWNX_ERROR,
    /// Error invalid VIF index parameter
    CFGRWNX_INVALID_VIF,
    /// Error invalid STA index parameter
    CFGRWNX_INVALID_STA,
    /// Error invalid parameter
    CFGRWNX_INVALID_PARAM,
};

/// CFGRWNX message header
struct cfgrwnx_msg_hdr
{
	union {
        /// For CFGRWNX commands, queue handle to use to push the response
        rtos_queue resp_queue;
        /// Used to store context of producer
        void *context;
	};
    /// ID of the message.
    uint16_t id;
    /// Length, in bytes, of the message (including this header)
    uint16_t len;
    #if defined(NX_SUPPORT_RTOS_HOST) || defined(NX_SUPPORT_AMP_IPC) || defined(CFG_AMP_IPC)
    uint16_t dest_task_id;
    uint16_t src_task_id;
    #endif
};

/// CFGRWNX generic message structure
struct cfgrwnx_msg
{
    /// header
    struct cfgrwnx_msg_hdr hdr;
};

/// CFGRWNX generic response structure
struct cfgrwnx_resp
{
    /// header
    struct cfgrwnx_msg_hdr hdr;
    /// Status
    uint32_t status;
};
#if defined(NX_SUPPORT_AMP_IPC) || defined(CFG_AMP_IPC)
struct ipc_msg_hdr
{
    uint16_t id;
    uint16_t len;
    uint32_t data[];
};

struct ipc_c2a_msg
{
    uint16_t id;
    uint16_t len;
    uint8_t data[IPC_C2A_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr)]; ///< Message data
};

struct ipc_a2c_msg
{
    uint16_t id;
    uint16_t len;
    uint8_t data[IPC_A2C_MSG_BUF_SIZE - sizeof(struct ipc_msg_hdr)]; ///< Message data
};

enum {
    NET_EVENT_UP,   /*net up*/
    NET_EVENT_DOWN, /*net down*/
    NET_EVENT_DHCP_START,
    NET_EVENT_DHCP_STOP,
    NET_EVENT_DHCP_RELEASE
};

enum {
    IPC_IND_NETIF, /*netif*/
    IPC_IND_PRINT,    /*print msg, err ind*/
    IPC_IND_EVENT, /*system event*/
};

struct cfg_ind_netif
{
    struct ipc_msg_hdr hdr;
    /// Vif idx
    uint16_t fvif_idx;
    /// event code
    uint16_t evt;
};

struct cfg_ind_event
{
    struct ipc_msg_hdr hdr;
    event_module_t module_id;
    int32_t event_id;
    void *event_data;
    int32_t event_data_size;
};

struct cfg_conf
{
    struct cfgrwnx_msg_hdr hdr;
    uint8_t macaddr[6];
};

struct cfg_conf_resp
{
    struct cfgrwnx_msg_hdr hdr;
    uint32_t status;
};
#endif
#endif // _CFG_CMD_
