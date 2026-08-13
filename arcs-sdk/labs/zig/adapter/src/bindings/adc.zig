///! ARCS SDK ADC 驱动 FFI 绑定
///! 映射 lisa_adc.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// ADC 枚举
// ════════════════════════════════════════════════════════════════════
pub const AdcReference = enum(u32) {
    vdd_1v2 = 0,
    vdd_3v6 = 1,
    vdd_io_auto = 2,
    vdd_io_auto_mul3 = 3,
    external = 4,
};

pub const AdcResolution = enum(u32) {
    resolution_10bit = 10,
};

// ════════════════════════════════════════════════════════════════════
// ADC 结构体
// ════════════════════════════════════════════════════════════════════
pub const ChannelConfig = extern struct {
    reference: AdcReference = .vdd_io_auto,
    resolution: AdcResolution = .resolution_10bit,
};

// ════════════════════════════════════════════════════════════════════
// ADC API 虚表
// ════════════════════════════════════════════════════════════════════
pub const AdcApi = extern struct {
    read_val: ?*const fn (*device.Device, u32, *u16) callconv(.C) i32,
    channel_setup: ?*const fn (*device.Device, u32, *const ChannelConfig) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 辅助函数
// ════════════════════════════════════════════════════════════════════

pub fn channelSetup(dev: *device.Device, channel: u32, config: *const ChannelConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.channel_setup orelse return -6;
    return func(dev, channel, config);
}

pub fn readValue(dev: *device.Device, channel: u32, value: *u16) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read_val orelse return -6;
    return func(dev, channel, value);
}

/// 将 ADC 原始值转为毫伏 (参考 LISA_ADC_RAW_TO_MV 宏)
pub fn rawToMv(raw: i32, ref_mv: u32, resolution: u32) i32 {
    return @intCast(@divTrunc(@as(i64, raw) * @as(i64, ref_mv), (@as(i64, 1) << @intCast(resolution)) - 1));
}

fn getApi(dev: *device.Device) ?*const AdcApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
