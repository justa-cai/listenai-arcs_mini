#ifndef __EVENT_H__
#define __EVENT_H__

#ifdef __cplusplus
 extern "C" {
#endif

typedef void (*event_handler_t)(char *buf, int buf_len, int flags, void* handler_user_data );

typedef struct
{
	event_handler_t	handler;
	void*	handler_user_data;
} event_list_elem_t;	 
	 
typedef enum  {
	OP_OK = 0,
	MULTIPLE_REGIST,
	ILLEGAL_EVENT,
	MAX_ROW_LIMIT,
	NOT_FOUND_REGIST,
}EVENT_REGISTAPI_RETURN;

typedef enum {
    // Basic initialization and device info
    EVENT_ATCMD_BLE_INIT = 0,
    
    // Advertising
    EVENT_ATCMD_BLE_ADV_PARAM,
    EVENT_ATCMD_BLE_ADV_DATA,
    EVENT_ATCMD_BLE_ADV_SCANRSP_DATA,
	EVENT_ATCMD_BLE_ADV_START,
    EVENT_ATCMD_BLE_ADV_STOP,
    EVENT_ATCMD_BLE_ADV_GEN,
    EVENT_ATCMD_BLE_ADV_STATE,
    
    // Scanning
    EVENT_ATCMD_BLE_SCAN_PARAM,
    EVENT_ATCMD_BLE_SCAN_START,
    EVENT_ATCMD_BLE_SCAN_STOP,
    
    // Connection management
    EVENT_ATCMD_BLE_CONNECT,
    EVENT_ATCMD_BLE_CONNECT_CANCEL,
    EVENT_ATCMD_BLE_CONNECT_UPDATE,
    EVENT_ATCMD_BLE_DISCONNECT,
    EVENT_ATCMD_BLE_GET_CONN_INFO,
    EVENT_ATCMD_BLE_GET_DEV_INFO,
    
    // Security and pairing
    EVENT_ATCMD_BLE_AUTH_REQ,
    EVENT_ATCMD_BLE_KEY_CFM,
    EVENT_ATCMD_BLE_DELETE_BOND,
    EVENT_ATCMD_BLE_PAIRED_LIST,
    
    // Advanced configuration
    EVENT_ATCMD_BLE_SET_PHY,
    EVENT_ATCMD_BLE_MTU_EXCH,
    EVENT_ATCMD_BLE_EXIT_LATENCY,
    EVENT_ATCMD_BLE_ENTRY_LATENCY,
    EVENT_ATCMD_BLE_SET_CON_PARAM_DIS,
    EVENT_ATCMD_BLE_SET_CON_EXIT_LATENCY,
    
    // Whitelist and advanced advertising
    EVENT_ATCMD_BLE_WHITELIST,
    EVENT_ATCMD_BLE_PER_SYNC_START,
    EVENT_ATCMD_BLE_PER_SYNC_STOP,
    
    // Test mode
    EVENT_ATCMD_BLE_TEST_TX,
    EVENT_ATCMD_BLE_TEST_RX,
    EVENT_ATCMD_BLE_TEST_STOP,
    
    // Low priority
    EVENT_ATCMD_BLE_SET_PUB_ADDR,
    
	EVENT_ATCMD_BLE_MAX,
}EVENT_ATCMD_BLE_INDICATE_TYPE;

void init_event_callback_list();
int unreg_event_handler(unsigned int event_cmds, event_handler_t handler_func);
int reg_event_handler(unsigned int event_cmds, event_handler_t handler_func, void *handler_user_data);
int indicate_event_handle(unsigned int event_cmd, char *buf, int buf_len, int flags);

#ifdef __cplusplus
}
#endif

#endif
