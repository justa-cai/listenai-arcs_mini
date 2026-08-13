///! ARCS SDK Bluetooth 组件 FFI 绑定
///! 映射 lisa_bluetooth.h
const c = @cImport({
    @cInclude("stdint.h");
    @cInclude("stdbool.h");
    @cInclude("lisa_bluetooth.h");
    @cInclude("lisa_bluetooth_gap.h");
    @cInclude("lisa_ble_api.h");
    @cInclude("netcfg_ble.h");
});

pub const c_api = c;

// ════════════════════════════════════════════════════════════════════
// 蓝牙枚举/常量
// ════════════════════════════════════════════════════════════════════

pub const DiscoveryType = c.gapm_disc_type_e;
pub const DISCOVERY_GENERAL: DiscoveryType = c.GAPM_DISC_TYPE_GEN_DISC;
pub const DISCOVERY_LIMITED: DiscoveryType = c.GAPM_DISC_TYPE_LIM_DISC;

pub const BtProfile = enum(i32) {
    a2dp = 0,
    hfp = 1,
};

// ════════════════════════════════════════════════════════════════════
// 蓝牙地址结构
// ════════════════════════════════════════════════════════════════════

pub const BdAddr = c.gap_bdaddr_t;

pub const MAX_DISCOVERED_DEVICES: comptime_int = c.MAX_DISCOVERED_DEVICES;

pub const GAP_AD_TYPE_MANUFACTURER_SPECIFIC: u8 = c.GAP_AD_TYPE_MANUFACTURER_SPECIFIC;
pub const GAP_AD_TYPE_LOCAL_NAME_COMPLETE: u8 = c.GAP_AD_TYPE_LOCAL_NAME_COMPLETE;

pub const ADV_GEN: u8 = c.LISA_BLE_ADV_GEN;
pub const ADV_GEN_PAIRED: u8 = c.LISA_BLE_ADV_GEN_PAIRED;
pub const ADV_DIR: u8 = c.LISA_BLE_ADV_DIR;
pub const ADV_DIR_HDC: u8 = c.LISA_BLE_ADV_DIR_HDC;

pub const NETCFG_AUTH_INFO: u16 = c.NETCFG_BLE_AUTH_INFO;
pub const NETCFG_SUCCESS: u16 = c.NETCFG_BLE_SUCCESS;
pub const NETCFG_ERR: u16 = c.NETCFG_BLE_ERR;

pub const DiscoveryInfo = c.lisa_bt_discovery_info_t;

// ════════════════════════════════════════════════════════════════════
// 回调类型
// ════════════════════════════════════════════════════════════════════

pub const DiscoveryCallback = c.lisa_bt_discovery_callback_t;
pub const EnableCmpCallback = c.lisa_bluetooth_enable_cmp_cb_t;
pub const BleConnCallback = c.lisa_ble_conn_cb_t;
pub const BleDiscCallback = c.lisa_ble_disc_cb_t;
pub const BleBondCallback = c.lisa_ble_bond_cb_t;
pub const BleKeyReqCallback = c.lisa_ble_key_req_cb_t;
pub const BtClassicConnCallback = c.lisa_bt_classic_conn_cb_t;
pub const BtClassicDiscCallback = c.lisa_bt_classic_disc_cb_t;
pub const BtClassicAvrcpCallback = c.lisa_bt_classic_avrcp_cb_t;
pub const BtClassicProfileCallback = c.lisa_bt_classic_profile_cb_t;
pub const BleAddr = c.lisa_ble_addr_t;
pub const NetcfgHandler = c.lisa_ble_netcfg_handler_t;
pub const NetcfgCustomOpHandler = c.lisa_ble_netcfg_custom_op_handler_t;

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

// 初始化
pub const lisa_bluetooth_init = c.lisa_bluetooth_init;

// BLE 发现与连接
pub const lisa_bluetooth_inquiry_start = c.lisa_bluetooth_inquiry_start;
pub const lisa_bluetooth_connect_by_name = c.lisa_bluetooth_connect_by_name;
pub const lisa_bluetooth_connect_by_index = c.lisa_bluetooth_connect_by_index;
pub const lisa_bluetooth_connect_by_addr = c.lisa_bluetooth_connect_by_addr;
pub const lisa_bluetooth_disconnect_by_addr = c.lisa_bluetooth_disconnect_by_addr;
pub const lisa_bluetooth_disconnect_by_name = c.lisa_bluetooth_disconnect_by_name;
pub const lisa_bluetooth_disconnect_by_index = c.lisa_bluetooth_disconnect_by_index;
pub const lisa_bluetooth_get_discovered_devices = c.lisa_bluetooth_get_discovered_devices;
pub const lisa_bluetooth_clear_discovered_devices = c.lisa_bluetooth_clear_discovered_devices;

// 配对记录
pub const pairedListGet = c.bt_paired_list_get;
pub const pairedNameGet = c.bt_paired_name_get;
pub const pairedRemove = c.bt_paired_remove;

// BLE 回调注册
pub const lisa_bluetooth_register_discovery_callback = c.lisa_bluetooth_register_discovery_callback;
pub const lisa_ble_register_conn_cb = c.lisa_ble_register_conn_cb;
pub const lisa_ble_register_disc_cb = c.lisa_ble_register_disc_cb;
pub const lisa_ble_register_bond_cb = c.lisa_ble_register_bond_cb;
pub const lisa_ble_register_key_req_cb = c.lisa_ble_register_key_req_cb;
pub const lisa_ble_key_confirm = c.lisa_ble_key_confirm;

// BT Classic 回调注册
pub const lisa_bt_classic_register_conn_cb = c.lisa_bt_classic_register_conn_cb;
pub const lisa_bt_classic_register_disc_cb = c.lisa_bt_classic_register_disc_cb;
pub const lisa_bt_classic_register_avrcp_cb = c.lisa_bt_classic_register_avrcp_cb;
pub const lisa_bt_classic_register_profile_cb = c.lisa_bt_classic_register_profile_cb;

pub const advStart = c.lisa_ble_adv_start;
pub const advStop = c.lisa_ble_adv_stop;
pub const scanStart = c.lisa_ble_scan_start;
pub const scanStop = c.lisa_ble_scan_stop;
pub const scanParam = c.lisa_ble_scan_param;
pub const connect = c.lisa_ble_connect;
pub const disconnect = c.lisa_ble_disconnect;
pub const connUpdate = c.lisa_ble_conn_update;
pub const hidSend = c.lisa_ble_hid_send;
pub const voiceDataSend = c.lisa_ble_voice_data_send;
pub const netcfgSendNotify = c.lisa_ble_netcfg_send_notify;
pub const netcfgSetHandler = c.lisa_ble_netcfg_set_handler;
pub const netcfgSetCustomOpHandler = c.lisa_ble_netcfg_set_custom_op_handler;
pub const netcfgSendCustomData = c.lisa_ble_netcfg_send_custom_data;
