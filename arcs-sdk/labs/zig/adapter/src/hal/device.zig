///! ARCS SDK Zig 设备框架封装
///!
///! 用法:
///!   const dev = arcs.Device.get("uart0") orelse return error.NotFound;
///!   if (dev.isReady()) { ... }
///!
///!   // 遍历所有设备
///!   arcs.Device.forEach(struct {
///!       pub fn callback(d: *arcs.Device.Raw) bool {
///!           log.info("Device: {s}", .{d.name()});
///!           return true; // continue
///!       }
///!   }.callback);
const std = @import("std");
const c_device = @import("../bindings/device.zig");

pub const Raw = c_device.Device;

/// 按名称获取设备
pub fn get(dev_name: [:0]const u8) ?*c_device.Device {
    return c_device.lisa_device_get(dev_name.ptr);
}

/// 检查设备是否就绪
pub fn isReady(dev: *const c_device.Device) bool {
    return c_device.lisa_device_ready(dev);
}

/// 获取已注册设备总数
pub fn count() u32 {
    return c_device.lisa_device_get_count();
}

/// 获取设备名称
pub fn name(dev: *const c_device.Device) []const u8 {
    if (dev.name == null) return "(null)";
    return std.mem.span(@as([*:0]const u8, @ptrCast(dev.name)));
}

/// 获取设备统计信息
pub fn stats(dev: *const c_device.Device) c_device.DeviceStats {
    var s: c_device.DeviceStats = undefined;
    c_device.lisa_device_get_stats(dev, &s);
    return s;
}

/// 遍历所有设备
pub fn forEach(comptime callback: fn (*c_device.Device) bool) void {
    const Wrapper = struct {
        fn cb(dev: *c_device.Device, _: ?*anyopaque) callconv(.C) i32 {
            return if (callback(dev)) 0 else 1;
        }
    };
    _ = c_device.lisa_device_foreach(Wrapper.cb, null);
}

/// 初始化设备框架
pub fn initAll() !void {
    var ret = c_device.lisa_device_early_init();
    if (ret != 0) return error.EarlyInitFailed;
    ret = c_device.lisa_device_init();
    if (ret != 0) return error.InitFailed;
}
