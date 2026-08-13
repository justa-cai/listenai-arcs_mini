///! ARCS SDK Zig WiFi HAL
///!
///! Provides a clean Zig API over the WiFi Manager, MAC Manager,
///! lisa_wifi init, and DHCP/network interface primitives.
///!
///! Usage:
///!   const arcs = @import("arcs");
///!
///!   // 1. Init MAC manager
///!   try arcs.WiFi.macManagerInit(.{});
///!
///!   // 2. Register DHCP callback
///!   arcs.WiFi.onDhcpStatus(myDhcpCb);
///!
///!   // 3. Start WiFi (async - calls init_done when ready)
///!   try arcs.WiFi.init(.{ .on_init_done = onReady });
///!
///!   // In onReady callback:
///!   try arcs.WiFi.managerInit();
///!   try arcs.WiFi.staEnable();
///!   try arcs.WiFi.onConnection(myConnCb);
///!   try arcs.WiFi.connect("MySSID", "MyPassword");

const c = @import("../bindings/wifi.zig");

pub const WiFi = struct {
    // ── Re-export binding types for user convenience ──────────────
    pub const EncryptionMode = c.EncryptionMode;
    pub const ConnectionStatus = c.ConnectionStatus;
    pub const StaConfig = c.StaConfig;
    pub const ScanInfo = c.ScanInfo;
    pub const ConnectionInfo = c.ConnectionInfo;
    pub const AutoConnConfig = c.AutoConnConfig;
    pub const ConnectionCb = c.ConnectionCb;
    pub const ScanDoneCb = c.ScanDoneCb;
    pub const DhcpStatusCb = c.DhcpStatusCb;
    pub const StorageSearchMode = c.StorageSearchMode;
    pub const WIFI_VIF_STA_IDX = c.WIFI_VIF_STA_IDX;

    // ── Errors ───────────────────────────────────────────────────
    pub const Error = error{
        WifiInitFailed,
        MacManagerInitFailed,
        WifiMgrInitFailed,
        StaEnableFailed,
        StaDisableFailed,
        ConnectFailed,
        DisconnectFailed,
        AddCallbackFailed,
        RemoveCallbackFailed,
        ScanFailed,
        AutoConnectStartFailed,
        AutoConnectStopFailed,
        StorageSaveFailed,
        StorageDeleteFailed,
        StorageSearchFailed,
        DhcpStartFailed,
        DhcpStopFailed,
        GetConnectedInfoFailed,
    };

    // ── Init Config ──────────────────────────────────────────────
    pub const InitConfig = struct {
        custom_mac: c.CustomMacFunc = null,
        on_init_done: c.WifiInitDoneCb = null,
    };

    // ── MAC Manager ──────────────────────────────────────────────

    pub const MacManagerConfig = struct {
        random_mac_if_mac_invalid: bool = false,
    };

    /// Stored MAC manager handle (module-level state, like the C sample)
    var mac_manager_handle: ?*c.MacManager = null;

    /// Initialize the MAC manager subsystem.
    /// Must be called before WiFi init so custom_mac callback can use it.
    pub fn macManagerInit(config: MacManagerConfig) Error!void {
        const ops = c.mac_manager_ops_get() orelse return Error.MacManagerInitFailed;
        var cfg = c.MacManagerConfig{
            .random_mac_if_mac_invalid = config.random_mac_if_mac_invalid,
        };
        mac_manager_handle = c.mac_manager_init(ops.mem_ops, ops.content_ops, &cfg);
        if (mac_manager_handle == null) return Error.MacManagerInitFailed;
    }

    /// Get the MAC address via the MAC manager.
    /// Returns the 6-byte MAC address.
    pub fn macManagerGetMac() Error![6]u8 {
        var mac: [6]u8 = undefined;
        const ret = c.mac_manager_get(mac_manager_handle, &mac, 6);
        if (ret != 0) return Error.MacManagerInitFailed;
        return mac;
    }

    /// Default custom_mac callback that uses the module-level MAC manager.
    /// Can be passed as InitConfig.custom_mac.
    pub fn defaultCustomMac(mac_addr: *[6]u8) callconv(.C) i8 {
        const ret = c.mac_manager_get(mac_manager_handle, mac_addr, 6);
        return @intCast(ret);
    }

    // ── WiFi Init (lisa_wifi) ────────────────────────────────────

    /// Initialize the WiFi subsystem (calls lisa_wifi_init).
    /// This is asynchronous; the on_init_done callback fires when WiFi HW is ready.
    pub fn init(config: InitConfig) Error!void {
        var ops = c.WifiOps{
            .custom_mac = config.custom_mac,
            .init_done = config.on_init_done,
        };
        const ret = c.lisa_wifi_init(&ops);
        if (ret != 0) return Error.WifiInitFailed;
    }

    // ── WiFi Manager ─────────────────────────────────────────────

    /// Initialize the WiFi manager (call in init_done callback).
    pub fn managerInit() Error!void {
        const ops = c.wifi_mgr_ops_get() orelse return Error.WifiMgrInitFailed;
        const ret = c.wifi_mgr_init(ops);
        if (ret != 0) return Error.WifiMgrInitFailed;
    }

    /// Deinitialize the WiFi manager.
    pub fn managerDeinit() Error!void {
        const ret = c.wifi_mgr_deinit();
        if (ret != 0) return Error.WifiMgrInitFailed;
    }

    /// Enable WiFi station mode.
    pub fn staEnable() Error!void {
        const ret = c.wifi_mgr_sta_enable();
        if (ret != 0) return Error.StaEnableFailed;
    }

    /// Disable WiFi station mode.
    pub fn staDisable() Error!void {
        const ret = c.wifi_mgr_sta_disable();
        if (ret != 0) return Error.StaDisableFailed;
    }

    /// Check whether station mode is enabled.
    pub fn staIsEnabled() bool {
        return c.wifi_mgr_sta_is_enable();
    }

    /// Connect to an AP. Copies ssid/password into a StaConfig struct.
    /// Blocking by default (asynchronous=false).
    pub fn connect(ssid: []const u8, password: []const u8) Error!void {
        var cfg = StaConfig{};
        copySliceToArray(&cfg.ssid, ssid);
        copySliceToArray(&cfg.pwd, password);
        const ret = c.wifi_mgr_sta_connect(&cfg, false);
        if (ret != 0) return Error.ConnectFailed;
    }

    /// Connect with a full StaConfig (advanced usage).
    pub fn connectWithConfig(cfg: *StaConfig, asynchronous: bool) Error!void {
        const ret = c.wifi_mgr_sta_connect(cfg, asynchronous);
        if (ret != 0) return Error.ConnectFailed;
    }

    /// Disconnect from the current AP.
    pub fn disconnect() Error!void {
        const ret = c.wifi_mgr_sta_disconnect(false);
        if (ret != 0) return Error.DisconnectFailed;
    }

    /// Get the current connection status.
    pub fn getStatus() ConnectionStatus {
        return c.wifi_mgr_sta_get_status();
    }

    /// Get connected AP information.
    pub fn getConnectedInfo() Error!StaConfig {
        var info: StaConfig = .{};
        const ret = c.wifi_mgr_sta_get_connected_info(&info);
        if (ret != 0) return Error.GetConnectedInfoFailed;
        return info;
    }

    /// Register a connection status change callback.
    pub fn onConnection(cb: ConnectionCb) Error!void {
        const ret = c.wifi_mgr_sta_add_connection_cb(cb, null);
        if (ret != 0) return Error.AddCallbackFailed;
    }

    /// Register a connection callback with user argument.
    pub fn onConnectionWithArg(cb: ConnectionCb, arg: ?*anyopaque) Error!void {
        const ret = c.wifi_mgr_sta_add_connection_cb(cb, arg);
        if (ret != 0) return Error.AddCallbackFailed;
    }

    /// Remove a connection callback.
    pub fn removeConnectionCb(cb: ConnectionCb) Error!void {
        const ret = c.wifi_mgr_sta_remove_connection_cb(cb);
        if (ret != 0) return Error.RemoveCallbackFailed;
    }

    // ── Scanning ─────────────────────────────────────────────────

    /// Scan for nearby APs (synchronous). Returns the number of APs found.
    /// Results are written into the provided slice.
    pub fn scan(results: []ScanInfo) Error!usize {
        const ret = c.wifi_mgr_scan_ap(
            if (results.len > 0) results.ptr else null,
            @intCast(results.len),
            false,
        );
        if (ret < 0) return Error.ScanFailed;
        return @intCast(ret);
    }

    /// Register a scan-done callback (for async scanning).
    pub fn onScanDone(cb: ScanDoneCb) Error!void {
        const ret = c.wifi_mgr_add_scan_done_cb(cb, null);
        if (ret != 0) return Error.AddCallbackFailed;
    }

    /// Remove a scan-done callback.
    pub fn removeScanDoneCb(cb: ScanDoneCb) Error!void {
        const ret = c.wifi_mgr_remove_scan_done_cb(cb);
        if (ret != 0) return Error.RemoveCallbackFailed;
    }

    // ── Auto-connect ─────────────────────────────────────────────

    /// Start WiFi auto-connect with the given configuration.
    pub fn autoConnectStart(config: AutoConnConfig) Error!void {
        var cfg = config;
        const ret = c.wifi_mgr_auto_connect_start(&cfg);
        if (ret != 0) return Error.AutoConnectStartFailed;
    }

    /// Stop WiFi auto-connect.
    pub fn autoConnectStop() Error!void {
        const ret = c.wifi_mgr_auto_connect_stop();
        if (ret != 0) return Error.AutoConnectStopFailed;
    }

    // ── NVS Storage ──────────────────────────────────────────────

    /// Save an AP to NVS storage.
    pub fn storageSaveAp(cfg: *StaConfig) Error!void {
        const ret = c.wifi_mgr_storage_save_ap(cfg);
        if (ret != 0) return Error.StorageSaveFailed;
    }

    /// Delete an AP from NVS storage.
    pub fn storageDeleteAp(cfg: *StaConfig) Error!void {
        const ret = c.wifi_mgr_storage_delete_ap(cfg);
        if (ret != 0) return Error.StorageDeleteFailed;
    }

    /// Search for saved APs in NVS storage. Returns the number found.
    pub fn storageSearchAp(results: []StaConfig, mode: StorageSearchMode, target: ?*anyopaque) Error!usize {
        const ret = c.wifi_mgr_storage_search_ap(
            if (results.len > 0) results.ptr else null,
            @intCast(results.len),
            mode,
            target,
        );
        if (ret < 0) return Error.StorageSearchFailed;
        return @intCast(ret);
    }

    // ── DHCP / Network Interface ─────────────────────────────────

    /// Start DHCP on the STA interface (vif_idx=0).
    pub fn dhcpStart() Error!void {
        const ret = c.ls_dhcpc_start(WIFI_VIF_STA_IDX);
        if (ret != 0) return Error.DhcpStartFailed;
    }

    /// Stop DHCP on the STA interface.
    pub fn dhcpStop() Error!void {
        const ret = c.ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        if (ret != 0) return Error.DhcpStopFailed;
    }

    /// Register a DHCP status callback.
    pub fn onDhcpStatus(cb: DhcpStatusCb) void {
        c.net_dhcp_register_status_callback(cb, null);
    }

    /// Register a DHCP status callback with user argument.
    pub fn onDhcpStatusWithArg(cb: DhcpStatusCb, arg: ?*anyopaque) void {
        c.net_dhcp_register_status_callback(cb, arg);
    }

    /// Bring the STA network interface up.
    pub fn netIfUp() void {
        const net_if = c.net_if_get(WIFI_VIF_STA_IDX);
        if (net_if) |nif| {
            c.net_if_up(nif);
        }
    }

    /// Bring the STA network interface down.
    pub fn netIfDown() void {
        const net_if = c.net_if_get(WIFI_VIF_STA_IDX);
        if (net_if) |nif| {
            c.net_if_down(nif);
        }
    }

    // ── Helpers ──────────────────────────────────────────────────

    /// Copy a Zig slice into a fixed-size C array, zero-filling the rest.
    fn copySliceToArray(dest: []u8, src: []const u8) void {
        const copy_len = if (src.len < dest.len) src.len else dest.len;
        for (dest[0..copy_len], src[0..copy_len]) |*d, s| {
            d.* = s;
        }
        // Zero-fill remainder
        for (dest[copy_len..]) |*d| {
            d.* = 0;
        }
    }

    /// Extract a null-terminated string from a fixed C char array.
    pub fn cStrFromArray(arr: []const u8) []const u8 {
        for (arr, 0..) |ch, i| {
            if (ch == 0) return arr[0..i];
        }
        return arr;
    }
};
