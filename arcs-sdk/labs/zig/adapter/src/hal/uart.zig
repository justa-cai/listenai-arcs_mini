///! ARCS SDK Zig UART 驱动封装
///! 实现 std.io.Writer / std.io.Reader 接口
///!
///! 用法:
///!   var uart = try arcs.Uart.open("uart0");
///!   try uart.configure(.{ .baudrate = 115200 });
///!
///!   // 同步读写
///!   try uart.writeAll("Hello ARCS!\r\n");
///!   var buf: [64]u8 = undefined;
///!   const n = try uart.readTimeout(&buf, 1000);
///!
///!   // 作为 std.io.Writer 使用
///!   const writer = uart.writer();
///!   try std.fmt.format(writer, "Value: {d}\n", .{42});
const std = @import("std");
const c_uart = @import("../bindings/uart.zig");
const c_device = @import("../bindings/device.zig");

pub const Uart = struct {
    dev: *c_device.Device,

    const Self = @This();

    // ── 打开设备 ─────────────────────────────────────────────────

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    // ── 配置 ─────────────────────────────────────────────────────

    pub const Config = struct {
        baudrate: u32 = 115200,
        data_bits: c_uart.DataBits = .eight,
        stop_bits: c_uart.StopBits = .one,
        parity: c_uart.Parity = .none,
        flow_ctrl: c_uart.FlowControl = .none,
        transfer_mode: c_uart.TransferMode = .interrupt,
    };

    pub fn configure(self: Self, config: Config) !void {
        const c_config = c_uart.Config{
            .baudrate = config.baudrate,
            .data_bits = config.data_bits,
            .stop_bits = config.stop_bits,
            .parity = config.parity,
            .flow_ctrl = config.flow_ctrl,
            .transfer_mode = config.transfer_mode,
        };
        const ret = c_uart.configure(self.dev, &c_config);
        if (ret != 0) return mapError(ret);
    }

    // ── 同步读写 ─────────────────────────────────────────────────

    /// 同步写入全部数据
    pub fn writeAll(self: Self, data: []const u8) !void {
        return self.writeTimeout(data, 5000);
    }

    /// 带超时同步写入
    pub fn writeTimeout(self: Self, data: []const u8, timeout_ms: u32) !void {
        const ret = c_uart.writeSync(self.dev, data, timeout_ms);
        if (ret < 0) return mapError(ret);
    }

    /// 带超时同步读取
    pub fn readTimeout(self: Self, buf: []u8, timeout_ms: u32) !usize {
        const ret = c_uart.readSync(self.dev, buf, timeout_ms);
        if (ret < 0) return mapError(ret);
        return @intCast(ret);
    }

    // ── 轮询 IO ─────────────────────────────────────────────────

    /// 轮询读取一个字节 (非阻塞)
    pub fn pollRead(self: Self) ?u8 {
        var byte: u8 = undefined;
        if (c_uart.pollIn(self.dev, &byte) == 0) return byte;
        return null;
    }

    /// 轮询写入一个字节
    pub fn pollWrite(self: Self, byte: u8) void {
        c_uart.pollOut(self.dev, byte);
    }

    // ── 接收控制 ─────────────────────────────────────────────────

    pub fn enableRx(self: Self) !void {
        const ret = c_uart.rxEnable(self.dev);
        if (ret != 0) return mapError(ret);
    }

    // ── 回调 ─────────────────────────────────────────────────────

    pub fn setCallback(self: Self, cb: c_uart.UartCallback, user_data: ?*anyopaque) !void {
        const ret = c_uart.setCallback(self.dev, cb, user_data);
        if (ret != 0) return mapError(ret);
    }

    // ── std.io.Writer 接口 ───────────────────────────────────────

    pub const Writer = std.io.Writer(*const Self, UartError, writerWrite);

    pub fn writer(self: *const Self) Writer {
        return .{ .context = self };
    }

    fn writerWrite(self: *const Self, bytes: []const u8) UartError!usize {
        self.writeAll(bytes) catch return error.IoError;
        return bytes.len;
    }

    // ── std.io.Reader 接口 ───────────────────────────────────────

    pub const Reader = std.io.Reader(*const Self, UartError, readerRead);

    pub fn reader(self: *const Self) Reader {
        return .{ .context = self };
    }

    fn readerRead(self: *const Self, buf: []u8) UartError!usize {
        const n = self.readTimeout(buf, 1000) catch return error.IoError;
        return n;
    }
};

// ════════════════════════════════════════════════════════════════════
// 错误类型
// ════════════════════════════════════════════════════════════════════
pub const UartError = error{
    DeviceNotFound,
    DeviceNotReady,
    InvalidParam,
    NotSupported,
    Timeout,
    Busy,
    IoError,
    FrameError,
    ParityError,
    Overrun,
    Unknown,
};

fn mapError(code: i32) UartError {
    return switch (code) {
        -1 => error.InvalidParam,
        -6 => error.NotSupported,
        -7 => error.Timeout,
        -8 => error.Busy,
        -10 => error.IoError,
        else => error.Unknown,
    };
}
