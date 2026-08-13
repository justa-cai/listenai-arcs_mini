///! ARCS SDK Flash 驱动 FFI 绑定
///! 映射 lisa_flash.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// Flash 结构体
// ════════════════════════════════════════════════════════════════════

pub const FlashCapabilities = extern struct {
    no_explicit_erase: bool,
};

pub const FlashParameters = extern struct {
    write_block_size: u32,
    caps: FlashCapabilities,
    erase_value: u8,
};

pub const FlashPagesLayout = extern struct {
    pages_count: u32,
    pages_size: u32,
};

// ════════════════════════════════════════════════════════════════════
// Flash API 虚表
// ════════════════════════════════════════════════════════════════════
pub const FlashApi = extern struct {
    read: ?*const fn (*device.Device, usize, ?*anyopaque, usize) callconv(.C) i32,
    write: ?*const fn (*device.Device, usize, ?*const anyopaque, usize) callconv(.C) i32,
    erase: ?*const fn (*device.Device, usize, usize) callconv(.C) i32,
    sr_read: ?*const fn (*device.Device, u32, *u32) callconv(.C) i32,
    sr_write: ?*const fn (*device.Device, u32, u32) callconv(.C) i32,
    get_parameters: ?*const fn (*device.Device) callconv(.C) ?*const FlashParameters,
    page_layout: ?*const fn (*device.Device, *usize) callconv(.C) ?*const FlashPagesLayout,
};

// ════════════════════════════════════════════════════════════════════
// 内联 API 辅助
// ════════════════════════════════════════════════════════════════════

pub fn read(dev: *device.Device, offset: usize, data: []u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read orelse return -6;
    return func(dev, offset, data.ptr, data.len);
}

pub fn write(dev: *device.Device, offset: usize, data: []const u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write orelse return -6;
    return func(dev, offset, data.ptr, data.len);
}

pub fn erase(dev: *device.Device, offset: usize, size: usize) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.erase orelse return -6;
    return func(dev, offset, size);
}

pub fn getParameters(dev: *device.Device) ?*const FlashParameters {
    const api = getApi(dev) orelse return null;
    const func = api.get_parameters orelse return null;
    return func(dev);
}

fn getApi(dev: *device.Device) ?*const FlashApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
