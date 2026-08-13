///! Zig WiFi Scan Demo
///!
///! 扫描并打印附近的 WiFi AP 列表
///! 流程: lisa_wifi_init → wifi_sta_mode_enable → wifi_scan_start → wifi_sta_scanlist_dump

const arcs = @import("arcs");

// ── WiFi API (wifi_api.h) ───────────────────────────
extern fn wifi_sta_mode_enable() callconv(.C) i32;
extern fn wifi_scan_start(params: ?*WifiScanParams) callconv(.C) i32;
extern fn wifi_event_wait(id: u32, timeout_ms: u32) callconv(.C) i32;
extern fn wifi_event_clear(id: u32) callconv(.C) i32;
extern fn wifi_get_sta_mac(mac: *[6]u8) callconv(.C) i32;
extern fn wifi_get_sta_scanlist_nums(num: *i32) callconv(.C) i32;
extern fn wifi_sta_scanlist_dump(results: [*]WifiScanResult, tgt_num: i32, rel_num: *i32) callconv(.C) i32;

const EVENT_WIFI_SCAN_DONE: u32 = 5;

// lisa_wifi
const WifiOps = extern struct {
    custom_mac: ?*const fn (*[6]u8) callconv(.C) i8,
    init_done: ?*const fn () callconv(.C) void,
};
extern fn lisa_wifi_init(ops: *WifiOps) callconv(.C) i32;

// MAC manager
const MacManagerConfig = extern struct { random_mac_if_mac_invalid: bool };
const MacManagerOpsContainer = extern struct {
    mem_ops: ?*anyopaque,
    content_ops: ?*anyopaque,
};
extern fn mac_manager_ops_get() callconv(.C) ?*MacManagerOpsContainer;
extern fn mac_manager_init(mem: ?*anyopaque, content: ?*anyopaque, cfg: *MacManagerConfig) callconv(.C) ?*anyopaque;
extern fn mac_manager_get(handle: ?*anyopaque, mac: *[6]u8, len: u32) callconv(.C) i32;

// wifi_scan_params_t
const WifiScanParams = extern struct {
    ssid_len: u8 = 0,
    ssid_array: [32]u8 = [_]u8{0} ** 32,
    bssid: [6]u8 = [_]u8{0} ** 6,
    bssid_set_flag: u8 = 0,
    channel_cnt: u8 = 0,
    channel: [14]u8 = [_]u8{0} ** 14,
    scan_method: u32 = 0, // NORMAL_SCAN
    rssi_threshold: i8 = -127,
    duration: i32 = 0,
};

// wifi_scan_result_t
const WifiScanResult = extern struct {
    mode: u32,
    last_timestamp: u32,
    ssid_len: u8,
    ssid: [33]u8,
    bssid: [6]u8,
    channel: u8,
    rssi: i8,
    auth: u8,
    cipher: u8,
    gtk_cipher: u8,
    wps: u8,
    is_used: u8,
};

var mac_handle: ?*anyopaque = null;
var wifi_init_ready: bool = false;

fn customMac(mac: *[6]u8) callconv(.C) i8 {
    return @intCast(mac_manager_get(mac_handle, mac, 6));
}

fn wifiInitDone() callconv(.C) void {
    const wlog = arcs.log.scoped("zig-wifi");
    wlog.info("WiFi hardware ready!", .{});
    wifi_init_ready = true;
}

const WIFI_INIT_TIMEOUT_MS: u32 = 10000;

/// C 入口
export fn zig_wifi_demo_main() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    const wlog = arcs.log.scoped("zig-wifi");

    wlog.info("=== Zig WiFi Scan Demo ===", .{});

    // 1. MAC Manager
    const ops_container = mac_manager_ops_get() orelse {
        wlog.err("mac_manager_ops_get failed", .{});
        return -2;
    };
    var mac_cfg = MacManagerConfig{ .random_mac_if_mac_invalid = true };
    mac_handle = mac_manager_init(ops_container.mem_ops, ops_container.content_ops, &mac_cfg);
    if (mac_handle == null) {
        wlog.err("mac_manager_init failed", .{});
        return -3;
    }
    wlog.info("MAC manager OK", .{});

    // 2. WiFi init (异步)
    var wifi_ops = WifiOps{
        .custom_mac = customMac,
        .init_done = wifiInitDone,
    };
    if (lisa_wifi_init(&wifi_ops) != 0) {
        wlog.err("lisa_wifi_init failed", .{});
        return -4;
    }
    wlog.info("WiFi init started...", .{});

    return 0;
}

/// 在主线程执行扫描 (可阻塞)
export fn zig_wifi_scan_task() callconv(.C) i32 {
    const wlog = arcs.log.scoped("zig-wifi");

    // 等 WiFi 就绪
    var waited_ms: u32 = 0;
    while (!wifi_init_ready) {
        if (waited_ms >= WIFI_INIT_TIMEOUT_MS) {
            wlog.err("WiFi init timeout after {d} ms", .{WIFI_INIT_TIMEOUT_MS});
            return -5;
        }
        arcs.sleep(100);
        waited_ms += 100;
    }

    // 读 MAC
    var mac: [6]u8 = undefined;
    if (wifi_get_sta_mac(&mac) == 0) {
        wlog.info("MAC: {x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}:{x:0>2}", .{
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        });
    }

    // 启用 STA
    if (wifi_sta_mode_enable() != 0) {
        wlog.err("wifi_sta_mode_enable failed", .{});
        return -1;
    }
    wlog.info("STA mode enabled", .{});

    // 开始扫描 (全频道)
    wlog.info("Starting WiFi scan...", .{});
    _ = wifi_event_clear(EVENT_WIFI_SCAN_DONE);

    var scan_params = WifiScanParams{};
    if (wifi_scan_start(&scan_params) != 0) {
        wlog.err("wifi_scan_start failed", .{});
        return -2;
    }

    // 等待扫描完成
    if (wifi_event_wait(EVENT_WIFI_SCAN_DONE, 10000) != 0) {
        wlog.err("Scan timeout", .{});
        return -3;
    }
    wlog.info("Scan done!", .{});

    // 获取扫描结果数量
    var ap_num: i32 = 0;
    _ = wifi_get_sta_scanlist_nums(&ap_num);
    wlog.info("Found {d} AP(s)", .{ap_num});

    if (ap_num <= 0) {
        wlog.info("No WiFi networks found", .{});
        return 0;
    }

    // 限制最多显示 20 个
    const max_display: i32 = if (ap_num > 20) 20 else ap_num;
    var results: [20]WifiScanResult = undefined;
    var real_num: i32 = 0;

    if (wifi_sta_scanlist_dump(&results, max_display, &real_num) != 0) {
        wlog.err("wifi_sta_scanlist_dump failed", .{});
        return -4;
    }

    // 打印列表
    wlog.info("", .{});
    wlog.info("╔════╦══════════════════════════════════╦═══════╦══════╦════════╗", .{});
    wlog.info("║ #  ║ SSID                             ║ RSSI  ║  CH  ║  AUTH  ║", .{});
    wlog.info("╠════╬══════════════════════════════════╬═══════╬══════╬════════╣", .{});

    var i: usize = 0;
    while (i < @as(usize, @intCast(real_num))) : (i += 1) {
        const r = &results[i];
        const ssid_slice = r.ssid[0..r.ssid_len];

        const auth_str: []const u8 = switch (r.auth) {
            0 => "OPEN",
            1 => "WEP",
            2 => "WPA",
            3 => "WPA2",
            4 => "WPA/2",
            5 => "WPA3",
            else => "????",
        };

        wlog.info("║ {d:>2} ║ {s:<32} ║ {d:>4}  ║  {d:>2}  ║ {s:<6} ║", .{
            i + 1,
            ssid_slice,
            r.rssi,
            r.channel,
            auth_str,
        });
    }

    wlog.info("╚════╩══════════════════════════════════╩═══════╩══════╩════════╝", .{});
    wlog.info("", .{});
    wlog.info("=== WiFi Scan Complete: {d} AP(s) displayed ===", .{real_num});

    return 0;
}
