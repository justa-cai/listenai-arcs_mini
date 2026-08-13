///! ARCS SDK Zig I2C 驱动封装
///!
///! 用法:
///!   var i2c = try arcs.I2c.open("i2c0");
///!   try i2c.configure(.{ .speed = .fast });
///!
///!   // 写寄存器
///!   try i2c.writeReg(0x50, 0x00, &[_]u8{0x42});
///!
///!   // 读数据
///!   var buf: [4]u8 = undefined;
///!   try i2c.read(0x50, &buf);
const std = @import("std");
const c_i2c = @import("../bindings/i2c.zig");
const c_device = @import("../bindings/device.zig");

pub const I2c = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    pub const Speed = c_i2c.I2cSpeed;

    pub const Config = struct {
        speed: Speed = .fast,
        master_mode: bool = true,
    };

    pub fn configure(self: Self, config: Config) !void {
        const c_config = c_i2c.I2cConfig{
            .speed = @intFromEnum(config.speed),
            .master_mode = config.master_mode,
        };
        const ret = c_i2c.configure(self.dev, &c_config);
        if (ret != 0) return mapError(ret);
    }

    /// 写入数据到设备地址
    pub fn write(self: Self, addr: u16, data: []const u8) !void {
        const ret = c_i2c.write(self.dev, addr, data);
        if (ret != 0) return mapError(ret);
    }

    /// 从设备地址读取数据
    pub fn read(self: Self, addr: u16, buf: []u8) !void {
        const ret = c_i2c.read(self.dev, addr, buf);
        if (ret != 0) return mapError(ret);
    }

    /// 写寄存器后读取 (常见的 I2C 读寄存器模式)
    pub fn writeRead(self: Self, addr: u16, write_buf: []const u8, read_buf: []u8) !void {
        var msgs = [2]c_i2c.I2cMsg{
            .{
                .addr = addr,
                .flags = 0x00, // write
                .len = @intCast(write_buf.len),
                .buf = @constCast(write_buf.ptr),
            },
            .{
                .addr = addr,
                .flags = 0x08, // read
                .len = @intCast(read_buf.len),
                .buf = read_buf.ptr,
            },
        };
        const ret = c_i2c.transfer(self.dev, &msgs);
        if (ret != 0) return mapError(ret);
    }

    /// 便捷: 写入寄存器地址后读取数据
    pub fn readReg(self: Self, addr: u16, reg: u8, buf: []u8) !void {
        try self.writeRead(addr, &[_]u8{reg}, buf);
    }

    /// 便捷: 写入寄存器地址 + 数据
    pub fn writeReg(self: Self, addr: u16, reg: u8, data: []const u8) !void {
        // 构造 [reg, data...] 缓冲区
        var write_buf: [33]u8 = undefined; // 1 byte reg + max 32 bytes data
        if (data.len > 32) return error.OutOfRange;
        write_buf[0] = reg;
        @memcpy(write_buf[1 .. 1 + data.len], data);
        try self.write(addr, write_buf[0 .. 1 + data.len]);
    }
};

pub const I2cError = error{
    DeviceNotFound,
    DeviceNotReady,
    InvalidParam,
    NotSupported,
    Timeout,
    Busy,
    IoError,
    Nack,
    OutOfRange,
    Unknown,
};

fn mapError(code: i32) I2cError {
    return switch (code) {
        -1 => error.InvalidParam,
        -6 => error.NotSupported,
        -7 => error.Timeout,
        -8 => error.Busy,
        -10 => error.IoError,
        -11 => error.OutOfRange,
        -13 => error.Nack,
        else => error.Unknown,
    };
}
