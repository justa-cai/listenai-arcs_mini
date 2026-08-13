///! ARCS SDK SPI 驱动 FFI 绑定
///! 映射 lisa_spi.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// SPI 枚举
// ════════════════════════════════════════════════════════════════════
pub const SpiMode = enum(u32) {
    mode_0 = 0, // CPOL=0, CPHA=0
    mode_1 = 1, // CPOL=0, CPHA=1
    mode_2 = 2, // CPOL=1, CPHA=0
    mode_3 = 3, // CPOL=1, CPHA=1
};

pub const BitOrder = enum(u32) {
    msb_first = 0,
    lsb_first = 1,
};

pub const TransferFlags = enum(u32) {
    software_cs = 0x00,
    hardware_cs = 0x01,
};

pub const SpiTransferMode = enum(u32) {
    dma = 0,
    interrupt = 1,
};

// ════════════════════════════════════════════════════════════════════
// SPI 结构体
// ════════════════════════════════════════════════════════════════════
pub const SpiConfig = extern struct {
    frequency: u32 = 1_000_000, // 1 MHz default
    mode: SpiMode = .mode_0,
    bit_order: BitOrder = .msb_first,
    data_bits: u8 = 8,
    flags: TransferFlags = .software_cs,
    master_mode: bool = true,
    tx_transfer_mode: SpiTransferMode = .interrupt,
    tx_dma_channel: u8 = 0xFF,
    rx_transfer_mode: SpiTransferMode = .interrupt,
    rx_dma_channel: u8 = 0xFF,
};

pub const SpiTransfer = extern struct {
    tx_buf: ?[*]const u8 = null,
    rx_buf: ?[*]u8 = null,
    len: u32 = 0,
};

pub const SpiTransferCallback = *const fn (?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// SPI API 虚表
// ════════════════════════════════════════════════════════════════════
pub const SpiApi = extern struct {
    configure: ?*const fn (*device.Device, *const SpiConfig) callconv(.C) i32,
    get_config: ?*const fn (*device.Device, *SpiConfig) callconv(.C) i32,
    transfer: ?*const fn (*device.Device, *const SpiTransfer) callconv(.C) i32,
    write: ?*const fn (*device.Device, [*]const u8, u32) callconv(.C) i32,
    read: ?*const fn (*device.Device, [*]u8, u32) callconv(.C) i32,
    register_callback: ?*const fn (*device.Device, SpiTransferCallback, ?*anyopaque) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联 API 辅助
// ════════════════════════════════════════════════════════════════════

pub fn configure(dev: *device.Device, config: *const SpiConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure orelse return -6;
    return func(dev, config);
}

pub fn doTransfer(dev: *device.Device, xfer: *const SpiTransfer) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.transfer orelse return -6;
    return func(dev, xfer);
}

pub fn write(dev: *device.Device, buf: []const u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write orelse return -6;
    return func(dev, buf.ptr, @intCast(buf.len));
}

pub fn read_buf(dev: *device.Device, buf: []u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read orelse return -6;
    return func(dev, buf.ptr, @intCast(buf.len));
}

fn getApi(dev: *device.Device) ?*const SpiApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
