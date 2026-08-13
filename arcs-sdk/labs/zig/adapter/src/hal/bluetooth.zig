///! ARCS SDK Zig Bluetooth 组件封装
///!
///! 用法:
///!   try arcs.Bluetooth.init(onReady);
///!   try arcs.Bluetooth.startDiscovery(.general, 10);
///!   const devices = arcs.Bluetooth.getDiscoveredDevices();
///!   try arcs.Bluetooth.connectByIndex(0);
const std = @import("std");
const c_bt = @import("../bindings/bluetooth.zig");

pub const Bluetooth = struct {
    pub const DiscoveryInfo = c_bt.DiscoveryInfo;
    pub const BdAddr = c_bt.BdAddr;
    pub const Profile = c_bt.BtProfile;

    /// 初始化蓝牙子系统
    pub fn init(on_ready: c_bt.EnableCmpCallback) !void {
        const ret = c_bt.lisa_bluetooth_init(on_ready);
        if (ret != 0) return error.BluetoothInitFailed;
    }

    // ── 设备发现 ────────────────────────────────────────────────

    pub const DiscoveryMode = c_bt.DiscoveryType;

    /// 开始扫描蓝牙设备
    pub fn startDiscovery(mode: DiscoveryMode, max_count: u8) !void {
        const ret = c_bt.lisa_bluetooth_inquiry_start(mode, max_count);
        if (ret != 0) return error.DiscoveryFailed;
    }

    /// 按名称连接设备
    pub fn connectByName(name: [:0]const u8) !void {
        const ret = c_bt.lisa_bluetooth_connect_by_name(name.ptr);
        if (ret != 0) return error.ConnectFailed;
    }

    /// 按索引连接设备
    pub fn connectByIndex(index: u8) !void {
        const ret = c_bt.lisa_bluetooth_connect_by_index(index);
        if (ret != 0) return error.ConnectFailed;
    }

    /// 清除已发现设备列表
    pub fn clearDiscovered() void {
        c_bt.lisa_bluetooth_clear_discovered_devices();
    }

    // ── BLE 回调注册 ────────────────────────────────────────────

    pub fn onDiscovery(cb: c_bt.DiscoveryCallback) void {
        c_bt.lisa_bluetooth_register_discovery_callback(cb);
    }

    pub fn onBleConnect(cb: c_bt.BleConnCallback) void {
        c_bt.lisa_ble_register_conn_cb(cb);
    }

    pub fn onBleDisconnect(cb: c_bt.BleDiscCallback) void {
        c_bt.lisa_ble_register_disc_cb(cb);
    }

    pub fn onBleBond(cb: c_bt.BleBondCallback) void {
        c_bt.lisa_ble_register_bond_cb(cb);
    }

    // ── BT Classic 回调注册 ─────────────────────────────────────

    pub fn onClassicConnect(cb: c_bt.BtClassicConnCallback) void {
        c_bt.lisa_bt_classic_register_conn_cb(cb);
    }

    pub fn onClassicDisconnect(cb: c_bt.BtClassicDiscCallback) void {
        c_bt.lisa_bt_classic_register_disc_cb(cb);
    }

    pub fn onAvrcpKey(cb: c_bt.BtClassicAvrcpCallback) void {
        c_bt.lisa_bt_classic_register_avrcp_cb(cb);
    }

    pub fn onProfileChange(cb: c_bt.BtClassicProfileCallback) void {
        c_bt.lisa_bt_classic_register_profile_cb(cb);
    }

    /// 确认 BLE 配对密钥
    pub fn confirmKey(conidx: u8, accept: bool, passkey: u32) void {
        c_bt.lisa_ble_key_confirm(conidx, if (accept) 1 else 0, passkey);
    }
};
