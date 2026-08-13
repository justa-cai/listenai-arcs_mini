///! ARCS SDK I2C 驱动 FFI 绑定
///! 映射 lisa_i2c.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// I2C 枚举和标志
// ════════════════════════════════════════════════════════════════════
pub const I2cFlags = enum(u8) {
    none = 0x00,
    no_start = 0x01,
    no_stop = 0x02,
    addr_10bit = 0x04,
    read = 0x08,
};

pub const I2cSpeed = enum(u32) {
    standard = 100000, // 100 kHz
    fast = 400000, // 400 kHz
    fast_plus = 1000000, // 1 MHz
};

// ════════════════════════════════════════════════════════════════════
// I2C 结构体
// ════════════════════════════════════════════════════════════════════
pub const I2cMsg = extern struct {
    addr: u16,
    flags: u8,
    len: u16,
    buf: [*]u8,
};

pub const I2cConfig = extern struct {
    speed: u32 = 400000,
    master_mode: bool = true,
    slave_addr: u16 = 0,
};

// ════════════════════════════════════════════════════════════════════
// I2C API 虚表
// ════════════════════════════════════════════════════════════════════
pub const I2cApi = extern struct {
    configure: ?*const fn (*device.Device, *const I2cConfig) callconv(.C) i32,
    get_config: ?*const fn (*device.Device, *I2cConfig) callconv(.C) i32,
    transfer: ?*const fn (*device.Device, [*]I2cMsg, u32) callconv(.C) i32,
    write: ?*const fn (*device.Device, u16, [*]const u8, u32) callconv(.C) i32,
    read: ?*const fn (*device.Device, u16, [*]u8, u32) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联 API 辅助
// ════════════════════════════════════════════════════════════════════

pub fn configure(dev: *device.Device, config: *const I2cConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure orelse return -6;
    return func(dev, config);
}

pub fn transfer(dev: *device.Device, msgs: []I2cMsg) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.transfer orelse return -6;
    return func(dev, msgs.ptr, @intCast(msgs.len));
}

pub fn write(dev: *device.Device, addr: u16, buf: []const u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write orelse return -6;
    return func(dev, addr, buf.ptr, @intCast(buf.len));
}

pub fn read(dev: *device.Device, addr: u16, buf: []u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read orelse return -6;
    return func(dev, addr, buf.ptr, @intCast(buf.len));
}

fn getApi(dev: *device.Device) ?*const I2cApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
