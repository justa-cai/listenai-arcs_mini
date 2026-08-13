///! ARCS SDK WiFi FFI bindings
///! Maps: lisa_wifi.h, wifi_manager.h, wifi_manager_wifi_ops.h,
///!        mac_manager.h, mac_manager_ops.h, net_al.h, net_ip.h

// ════════════════════════════════════════════════════════════════════
// lisa_wifi.h types & functions
// ════════════════════════════════════════════════════════════════════

/// Custom MAC address callback: int8_t (*)(uint8_t mac_addr[6])
pub const CustomMacFunc = ?*const fn (*[6]u8) callconv(.C) i8;

/// WiFi init-done callback: void (*)(void)
pub const WifiInitDoneCb = ?*const fn () callconv(.C) void;

/// lisa_wifi_ops_t
pub const WifiOps = extern struct {
    custom_mac: CustomMacFunc = null,
    init_done: WifiInitDoneCb = null,
};

pub extern fn lisa_wifi_init(ops: ?*WifiOps) callconv(.C) c_int;

// ════════════════════════════════════════════════════════════════════
// wifi_manager_wifi_ops.h enums & structs
// ════════════════════════════════════════════════════════════════════

/// wifi_mgr_wifi_encryption_mode_t
pub const EncryptionMode = enum(c_int) {
    auto = 0,
    open = 1,
    wep = 2,
    wpa_psk = 3,
    wpa2_psk = 4,
    wpa_wpa2_psk = 5,
    wpa2_enterprise = 6,
    wpa3_psk = 7,
    wpa2_wpa3_psk = 8,
    unknown = 9,
    max = 10,
};

/// wifi_mgr_wifi_sta_config_t  (136 bytes on 32-bit)
/// Fields: char ssid[32], char pwd[64], char bssid[18], int channel,
///         int rssi, wifi_mgr_wifi_encryption_mode_t encryption_mode,
///         uint8_t pmk[32], uint8_t pmk_valid
pub const StaConfig = extern struct {
    ssid: [32]u8 = [_]u8{0} ** 32,
    pwd: [64]u8 = [_]u8{0} ** 64,
    bssid: [18]u8 = [_]u8{0} ** 18,
    channel: c_int = 0,
    rssi: c_int = 0,
    encryption_mode: EncryptionMode = .auto,
    pmk: [32]u8 = [_]u8{0} ** 32,
    pmk_valid: u8 = 0,
};

/// wifi_mgr_wifi_scan_info_t
/// Fields: char ssid[32], char bssid[18], int channel, int rssi,
///         wifi_mgr_wifi_encryption_mode_t encryption_mode
pub const ScanInfo = extern struct {
    ssid: [32]u8 = [_]u8{0} ** 32,
    bssid: [18]u8 = [_]u8{0} ** 18,
    channel: c_int = 0,
    rssi: c_int = 0,
    encryption_mode: EncryptionMode = .auto,
};

/// wifi_mgr_wifi_event_t (bitmask)
pub const WifiEvent = enum(c_int) {
    sta_connected = (1 << 0),
    sta_disconnected = (1 << 1),
    sta_connecting = (1 << 2),
    sta_connection_failed = (1 << 3),
    scan_done = (1 << 4),
    scan_failed = (1 << 5),
};

/// wifi_mgr_connect_fail_info_t
pub const ConnectFailInfo = extern struct {
    error_code: c_int = 0,
    status_code: c_int = 0,
    reason_code: c_int = 0,
};

// ════════════════════════════════════════════════════════════════════
// wifi_manager.h enums & structs
// ════════════════════════════════════════════════════════════════════

/// wifi_mgr_connection_status_t
pub const ConnectionStatus = enum(c_int) {
    connected = 0,
    connecting = 1,
    disconnected = 2,
    connect_failed = 3,
    max = 0xFF,
};

/// wifi_mgr_connection_info_t
pub const ConnectionInfo = extern struct {
    status: ConnectionStatus,
    sta_info: ?*StaConfig,
    reason: c_int,
};

/// wifi_mgr_autoconn_config_t
pub const AutoConnConfig = extern struct {
    interval_ms: u32 = 0,
    max_interval_ms: u32 = 0,
};

/// wifi_mgr_scan_ap_list_t
pub const ScanApList = extern struct {
    ap_info: ?[*]ScanInfo,
    count: u32,
};

/// wifi_mgr_storage_search_mode_t (bitmask)
pub const StorageSearchMode = enum(c_ulong) {
    search_all = (1 << 0),
    search_by_ssid = (1 << 1),
    search_by_bssid = (1 << 2),
    search_by_pwd = (1 << 3),
    search_by_channel = (1 << 4),
    search_by_encryption = (1 << 5),
};

/// WiFi connection callback: void (*)(wifi_mgr_connection_info_t *, void *)
pub const ConnectionCb = ?*const fn (?*ConnectionInfo, ?*anyopaque) callconv(.C) void;

/// WiFi scan done callback: void (*)(wifi_mgr_wifi_scan_info_t *, int, void *)
pub const ScanDoneCb = ?*const fn (?[*]ScanInfo, c_int, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// wifi_manager_ops.h
// ════════════════════════════════════════════════════════════════════

/// wifi_mgr_ops_t (opaque - contains pointers to os_ops, mem_ops, wifi_ops)
pub const WifiMgrOps = opaque {};

pub extern fn wifi_mgr_ops_get() callconv(.C) ?*WifiMgrOps;

// ════════════════════════════════════════════════════════════════════
// WiFi Manager extern functions (wifi_manager.h)
// ════════════════════════════════════════════════════════════════════

pub extern fn wifi_mgr_init(ops: ?*WifiMgrOps) callconv(.C) c_int;
pub extern fn wifi_mgr_deinit() callconv(.C) c_int;
pub extern fn wifi_mgr_sta_enable() callconv(.C) c_int;
pub extern fn wifi_mgr_sta_disable() callconv(.C) c_int;
pub extern fn wifi_mgr_sta_is_enable() callconv(.C) bool;
pub extern fn wifi_mgr_sta_connect(sta_config: ?*StaConfig, asynchronous: bool) callconv(.C) c_int;
pub extern fn wifi_mgr_sta_disconnect(asynchronous: bool) callconv(.C) c_int;
pub extern fn wifi_mgr_sta_get_status() callconv(.C) ConnectionStatus;
pub extern fn wifi_mgr_sta_get_connected_info(sta_info: ?*StaConfig) callconv(.C) c_int;
pub extern fn wifi_mgr_sta_add_connection_cb(connection_cb: ConnectionCb, arg: ?*anyopaque) callconv(.C) c_int;
pub extern fn wifi_mgr_sta_remove_connection_cb(connection_cb: ConnectionCb) callconv(.C) c_int;
pub extern fn wifi_mgr_scan_ap(ap_info: ?[*]ScanInfo, size: u32, asynchronous: bool) callconv(.C) c_int;
pub extern fn wifi_mgr_add_scan_done_cb(scandone_cb: ScanDoneCb, arg: ?*anyopaque) callconv(.C) c_int;
pub extern fn wifi_mgr_remove_scan_done_cb(scandone_cb: ScanDoneCb) callconv(.C) c_int;
pub extern fn wifi_mgr_storage_save_ap(ap_info: ?*StaConfig) callconv(.C) c_int;
pub extern fn wifi_mgr_storage_delete_ap(ap_info: ?*StaConfig) callconv(.C) c_int;
pub extern fn wifi_mgr_storage_search_ap(matched_list: ?[*]StaConfig, max_count: c_int, search_modes: StorageSearchMode, target: ?*anyopaque) callconv(.C) c_int;
pub extern fn wifi_mgr_auto_connect_start(autoconn_config: ?*AutoConnConfig) callconv(.C) c_int;
pub extern fn wifi_mgr_auto_connect_stop() callconv(.C) c_int;

// ════════════════════════════════════════════════════════════════════
// MAC Manager (mac_manager.h, mac_manager_ops.h)
// ════════════════════════════════════════════════════════════════════

/// mac_manager_config_t
pub const MacManagerConfig = extern struct {
    random_mac_if_mac_invalid: bool = false,
};

/// mac_manager_mem_ops_t
pub const MacManagerMemOps = extern struct {
    malloc_fn: ?*const fn (usize) callconv(.C) ?*anyopaque,
    free_fn: ?*const fn (?*anyopaque) callconv(.C) void,
};

/// mac_manager_content_ops_t
pub const MacManagerContentOps = extern struct {
    set: ?*const fn ([*]const u8, usize) callconv(.C) c_int,
    get: ?*const fn ([*]u8, *usize) callconv(.C) c_int,
    random: ?*const fn ([*]u8, *usize) callconv(.C) c_int,
    del: ?*const fn () callconv(.C) c_int,
};

/// mac_manager_ops_t (from mac_manager_ops.h)
pub const MacManagerOpsContainer = extern struct {
    mem_ops: ?*MacManagerMemOps,
    content_ops: ?*MacManagerContentOps,
};

/// mac_manager_t is opaque
pub const MacManager = opaque {};

pub extern fn mac_manager_init(mem_ops: ?*MacManagerMemOps, ops: ?*MacManagerContentOps, config: ?*MacManagerConfig) callconv(.C) ?*MacManager;
pub extern fn mac_manager_get(obj: ?*MacManager, mac_addr: [*]u8, mac_addr_len: usize) callconv(.C) c_int;
pub extern fn mac_manager_set(obj: ?*MacManager, mac_addr: [*]const u8, mac_addr_len: usize) callconv(.C) c_int;
pub extern fn mac_manager_del(obj: ?*MacManager) callconv(.C) c_int;
pub extern fn mac_manager_deinit(obj: ?*MacManager) callconv(.C) void;
pub extern fn mac_manager_ops_get() callconv(.C) ?*MacManagerOpsContainer;

// ════════════════════════════════════════════════════════════════════
// Network interface (net_al.h, net_ip.h)
// ════════════════════════════════════════════════════════════════════

/// net_if_t is opaque (its layout is stack-internal)
pub const NetIf = opaque {};

/// DHCP status callback: void (*)(int vif_idx, bool success, uint32_t ip, uint32_t mask, uint32_t gw, void *arg)
pub const DhcpStatusCb = ?*const fn (c_int, bool, u32, u32, u32, ?*anyopaque) callconv(.C) void;

// net_al.h functions
pub extern fn net_if_get(wifi_idx: c_int) callconv(.C) ?*NetIf;
pub extern fn net_if_up(net_if: ?*NetIf) callconv(.C) void;
pub extern fn net_if_down(net_if: ?*NetIf) callconv(.C) void;
pub extern fn net_dhcp_start(net_if: ?*NetIf) callconv(.C) c_int;
pub extern fn net_dhcp_stop(net_if: ?*NetIf) callconv(.C) void;
pub extern fn net_dhcp_register_status_callback(cb: DhcpStatusCb, arg: ?*anyopaque) callconv(.C) void;

// net_ip.h functions
pub extern fn ls_dhcpc_start(vif_idx: c_int) callconv(.C) c_int;
pub extern fn ls_dhcpc_stop(vif_idx: c_int) callconv(.C) c_int;
pub extern fn ls_get_ip(vif_idx: c_int, cfg: ?*anyopaque) callconv(.C) c_int;

/// WiFi VIF station index (from ls_wifi_type.h)
pub const WIFI_VIF_STA_IDX: c_int = 0;

// c_int and c_ulong are built-in primitives in Zig 0.13+
