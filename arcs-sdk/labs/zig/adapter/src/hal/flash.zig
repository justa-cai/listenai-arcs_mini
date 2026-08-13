///! ARCS SDK Zig Flash 驱动封装
///!
///! 用法:
///!   var flash = try arcs.Flash.open("flash0");
///!   var buf: [256]u8 = undefined;
///!   try flash.read(0x1000, &buf);
///!   try flash.erase(0x1000, 4096);
///!   try flash.write(0x1000, &data);
const c_flash = @import("../bindings/flash.zig");
const c_device = @import("../bindings/device.zig");

pub const Flash = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    /// 读取 Flash 数据
    pub fn read(self: Self, offset: usize, buf: []u8) !void {
        const ret = c_flash.read(self.dev, offset, buf);
        if (ret != 0) return mapError(ret);
    }

    /// 写入 Flash 数据 (写入前需确保区域已擦除)
    pub fn write(self: Self, offset: usize, data: []const u8) !void {
        const ret = c_flash.write(self.dev, offset, data);
        if (ret != 0) return mapError(ret);
    }

    /// 擦除 Flash 区域
    pub fn erase(self: Self, offset: usize, size: usize) !void {
        const ret = c_flash.erase(self.dev, offset, size);
        if (ret != 0) return mapError(ret);
    }

    /// 获取 Flash 参数 (写块大小, 擦除值等)
    pub fn getParameters(self: Self) ?*const c_flash.FlashParameters {
        return c_flash.getParameters(self.dev);
    }

    /// 获取最小写块大小
    pub fn writeBlockSize(self: Self) u32 {
        const params = self.getParameters() orelse return 1;
        return params.write_block_size;
    }
};

pub const FlashError = error{
    DeviceNotFound,
    DeviceNotReady,
    InvalidParam,
    NotSupported,
    IoError,
    Unknown,
};

fn mapError(code: i32) FlashError {
    return switch (code) {
        -1 => error.InvalidParam,
        -6 => error.NotSupported,
        -10 => error.IoError,
        else => error.Unknown,
    };
}
