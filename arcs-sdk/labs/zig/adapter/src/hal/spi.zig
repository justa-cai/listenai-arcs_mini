///! ARCS SDK Zig SPI 驱动封装
///!
///! 用法:
///!   var spi = try arcs.Spi.open("spi0");
///!   try spi.configure(.{ .frequency = 10_000_000, .mode = .mode_0 });
///!
///!   // 全双工传输
///!   var rx: [4]u8 = undefined;
///!   try spi.transferFull(&[_]u8{0x01, 0x02, 0x03, 0x04}, &rx);
///!
///!   // 只写
///!   try spi.write(&[_]u8{0xAA, 0xBB});
const std = @import("std");
const c_spi = @import("../bindings/spi.zig");
const c_device = @import("../bindings/device.zig");

pub const Spi = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    pub const Config = struct {
        frequency: u32 = 1_000_000,
        mode: c_spi.SpiMode = .mode_0,
        bit_order: c_spi.BitOrder = .msb_first,
        master_mode: bool = true,
    };

    pub fn configure(self: Self, config: Config) !void {
        const c_config = c_spi.SpiConfig{
            .frequency = config.frequency,
            .mode = config.mode,
            .bit_order = config.bit_order,
            .master_mode = config.master_mode,
        };
        const ret = c_spi.configure(self.dev, &c_config);
        if (ret != 0) return mapError(ret);
    }

    /// 全双工传输 (同时发送和接收)
    pub fn transferFull(self: Self, tx: []const u8, rx: []u8) !void {
        std.debug.assert(tx.len == rx.len);
        const xfer = c_spi.SpiTransfer{
            .tx_buf = tx.ptr,
            .rx_buf = rx.ptr,
            .len = @intCast(tx.len),
        };
        const ret = c_spi.doTransfer(self.dev, &xfer);
        if (ret != 0) return mapError(ret);
    }

    /// 只写 (忽略接收)
    pub fn write(self: Self, data: []const u8) !void {
        const ret = c_spi.write(self.dev, data);
        if (ret != 0) return mapError(ret);
    }

    /// 只读 (发送全零)
    pub fn read(self: Self, buf: []u8) !void {
        const ret = c_spi.read_buf(self.dev, buf);
        if (ret != 0) return mapError(ret);
    }

    /// 先写命令再读数据 (常见 SPI 设备模式)
    pub fn writeRead(self: Self, cmd: []const u8, read_buf: []u8) !void {
        try self.write(cmd);
        try self.read(read_buf);
    }
};

pub const SpiError = error{
    DeviceNotFound,
    DeviceNotReady,
    InvalidParam,
    NotSupported,
    Timeout,
    Busy,
    IoError,
    Unknown,
};

fn mapError(code: i32) SpiError {
    return switch (code) {
        -1 => error.InvalidParam,
        -6 => error.NotSupported,
        -7 => error.Timeout,
        -8 => error.Busy,
        -10 => error.IoError,
        else => error.Unknown,
    };
}
