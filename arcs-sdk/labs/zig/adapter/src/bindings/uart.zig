///! ARCS SDK UART 驱动 FFI 绑定
///! 映射 lisa_uart.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// UART 枚举类型
// ════════════════════════════════════════════════════════════════════
pub const Baudrate = enum(u32) {
    baud_1200 = 1200,
    baud_2400 = 2400,
    baud_4800 = 4800,
    baud_9600 = 9600,
    baud_19200 = 19200,
    baud_38400 = 38400,
    baud_57600 = 57600,
    baud_115200 = 115200,
    baud_230400 = 230400,
    baud_460800 = 460800,
    baud_921600 = 921600,
};

pub const DataBits = enum(u32) {
    five = 5,
    six = 6,
    seven = 7,
    eight = 8,
};

pub const StopBits = enum(u32) {
    one = 0,
    one_half = 1,
    two = 2,
};

pub const Parity = enum(u32) {
    none = 0,
    odd = 1,
    even = 2,
};

pub const FlowControl = enum(u32) {
    none = 0,
    rts_cts = 1,
    xon_xoff = 2,
};

pub const TransferMode = enum(u32) {
    interrupt = 0,
    dma = 1,
};

pub const Event = enum(u32) {
    rx_ready = 0x01,
    tx_done = 0x02,
    err = 0x04,
    break_detect = 0x08,
    overrun = 0x10,
    parity_error = 0x20,
    frame_error = 0x40,
    rx_timeout = 0x80,
};

// ════════════════════════════════════════════════════════════════════
// UART 结构体
// ════════════════════════════════════════════════════════════════════
pub const RxBufConfig = extern struct {
    buffer_count: u32 = 0, // 0=disabled, 2=ping-pong
    buffer_size: u32 = 0,
};

pub const Config = extern struct {
    baudrate: u32 = 115200,
    data_bits: DataBits = .eight,
    stop_bits: StopBits = .one,
    parity: Parity = .none,
    flow_ctrl: FlowControl = .none,
    transfer_mode: TransferMode = .interrupt,
    rx_buf_config: RxBufConfig = .{},
    dma_tx_channel: u8 = 0xFF, // auto
    dma_rx_channel: u8 = 0xFF, // auto
};

pub const UartCallback = *const fn (Event, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// UART API 虚表
// ════════════════════════════════════════════════════════════════════
pub const UartApi = extern struct {
    configure: ?*const fn (*device.Device, *const Config) callconv(.C) i32,
    get_config: ?*const fn (*device.Device, *Config) callconv(.C) i32,
    write_sync: ?*const fn (*device.Device, [*]const u8, u32, u32) callconv(.C) i32,
    read_sync: ?*const fn (*device.Device, [*]u8, u32, u32) callconv(.C) i32,
    rx_enable: ?*const fn (*device.Device) callconv(.C) i32,
    rx_disable: ?*const fn (*device.Device) callconv(.C) i32,
    poll_in: ?*const fn (*device.Device, *u8) callconv(.C) i32,
    poll_out: ?*const fn (*device.Device, u8) callconv(.C) void,
    flush: ?*const fn (*device.Device) callconv(.C) i32,
};

pub const UartApiAsync = extern struct {
    base: UartApi,
    write_async: ?*const fn (*device.Device, [*]const u8, u32) callconv(.C) i32,
    set_callback: ?*const fn (*device.Device, UartCallback, ?*anyopaque) callconv(.C) i32,
    write_abort: ?*const fn (*device.Device) callconv(.C) i32,
    get_tx_count: ?*const fn (*device.Device) callconv(.C) u32,
};

// ════════════════════════════════════════════════════════════════════
// 内联 API 辅助
// ════════════════════════════════════════════════════════════════════

pub fn configure(dev: *device.Device, config: *const Config) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure orelse return -6;
    return func(dev, config);
}

pub fn writeSync(dev: *device.Device, buf: []const u8, timeout_ms: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write_sync orelse return -6;
    return func(dev, buf.ptr, @intCast(buf.len), timeout_ms);
}

pub fn readSync(dev: *device.Device, buf: []u8, timeout_ms: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read_sync orelse return -6;
    return func(dev, buf.ptr, @intCast(buf.len), timeout_ms);
}

pub fn rxEnable(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.rx_enable orelse return -6;
    return func(dev);
}

pub fn pollIn(dev: *device.Device, byte: *u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.poll_in orelse return -6;
    return func(dev, byte);
}

pub fn pollOut(dev: *device.Device, byte: u8) void {
    const api = getApi(dev) orelse return;
    const func = api.poll_out orelse return;
    func(dev, byte);
}

pub fn flush(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.flush orelse return -6;
    return func(dev);
}

pub fn setCallback(dev: *device.Device, cb: UartCallback, user_data: ?*anyopaque) i32 {
    const api = getAsyncApi(dev) orelse return -6;
    const func = api.set_callback orelse return -6;
    return func(dev, cb, user_data);
}

fn getApi(dev: *device.Device) ?*const UartApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}

fn getAsyncApi(dev: *device.Device) ?*const UartApiAsync {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
