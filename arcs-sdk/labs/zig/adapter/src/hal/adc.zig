///! ARCS SDK Zig ADC 驱动封装
///!
///! 用法:
///!   var adc = try arcs.Adc.open("adc0");
///!   try adc.setupChannel(0, .{});
///!   const raw = try adc.read(0);
///!   const mv = adc.toMillivolts(raw, 3600, 10);
const c_adc = @import("../bindings/adc.zig");
const c_device = @import("../bindings/device.zig");

pub const Adc = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    pub const Reference = c_adc.AdcReference;
    pub const Resolution = c_adc.AdcResolution;

    pub const ChannelConfig = struct {
        reference: Reference = .vdd_io_auto,
        resolution: Resolution = .resolution_10bit,
    };

    pub fn setupChannel(self: Self, channel: u32, config: ChannelConfig) !void {
        const c_config = c_adc.ChannelConfig{
            .reference = config.reference,
            .resolution = config.resolution,
        };
        const ret = c_adc.channelSetup(self.dev, channel, &c_config);
        if (ret != 0) return error.SetupFailed;
    }

    /// 读取 ADC 原始值
    pub fn read(self: Self, channel: u32) !u16 {
        var value: u16 = 0;
        const ret = c_adc.readValue(self.dev, channel, &value);
        if (ret != 0) return error.ReadFailed;
        return value;
    }

    /// 将原始值转换为毫伏
    pub fn toMillivolts(raw: u16, ref_mv: u32, resolution_bits: u32) i32 {
        return c_adc.rawToMv(@as(i32, raw), ref_mv, resolution_bits);
    }
};
