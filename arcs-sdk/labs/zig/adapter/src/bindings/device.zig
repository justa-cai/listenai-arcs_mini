///! ARCS SDK 设备框架 FFI 绑定
///! 映射 lisa_device.h

// ════════════════════════════════════════════════════════════════════
// 设备统计信息
// ════════════════════════════════════════════════════════════════════
pub const DeviceStats = extern struct {
    ref_count: u32,
    init_result: i32,
    init_time: u32,
    init_timestamp: u32,
};

// ════════════════════════════════════════════════════════════════════
// 设备状态枚举
// ════════════════════════════════════════════════════════════════════
pub const DeviceState = enum(u32) {
    uninitialized = 0,
    initialized = 1,
    err = 2,
};

// ════════════════════════════════════════════════════════════════════
// 核心设备结构体
// ════════════════════════════════════════════════════════════════════
pub const Device = extern struct {
    name: ?[*]const u8,
    state: DeviceState,
    stats: DeviceStats,
    api: ?*anyopaque,
    priv_data: ?*anyopaque,
    user_data: ?*anyopaque,
    next: ?*Device,
};

// ════════════════════════════════════════════════════════════════════
// 设备迭代器回调
// ════════════════════════════════════════════════════════════════════
pub const DeviceIteratorCb = *const fn (*Device, ?*anyopaque) callconv(.C) i32;

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

/// 早期设备初始化 (heap 之前)
pub extern fn lisa_device_early_init() callconv(.C) i32;

/// 设备初始化 (heap 之后)
pub extern fn lisa_device_init() callconv(.C) i32;

/// 按名称获取设备
pub extern fn lisa_device_get(name: ?[*]const u8) callconv(.C) ?*Device;

/// 检查设备是否就绪
pub extern fn lisa_device_ready(dev: *const Device) callconv(.C) bool;

/// 获取已注册设备数量
pub extern fn lisa_device_get_count() callconv(.C) u32;

/// 获取设备统计信息
pub extern fn lisa_device_get_stats(dev: *const Device, stats: *DeviceStats) callconv(.C) void;

/// 重置设备统计信息
pub extern fn lisa_device_reset_stats(dev: *Device) callconv(.C) void;

/// 遍历所有设备
pub extern fn lisa_device_foreach(callback: DeviceIteratorCb, user_data: ?*anyopaque) callconv(.C) i32;
