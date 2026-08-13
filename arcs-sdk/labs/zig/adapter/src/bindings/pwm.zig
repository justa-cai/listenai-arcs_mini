///! ARCS SDK PWM 驱动 FFI 绑定
///! 映射 lisa_pwm.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// PWM 枚举
// ════════════════════════════════════════════════════════════════════
pub const PwmPolarity = enum(u32) {
    normal = 0,
    inverted = 1,
};

// ════════════════════════════════════════════════════════════════════
// PWM 结构体
// ════════════════════════════════════════════════════════════════════
pub const PwmConfig = extern struct {
    polarity: PwmPolarity = .normal,
};

// ════════════════════════════════════════════════════════════════════
// PWM API 虚表
// ════════════════════════════════════════════════════════════════════
pub const PwmApi = extern struct {
    enable: ?*const fn (*device.Device, u32) callconv(.C) i32,
    disable: ?*const fn (*device.Device, u32) callconv(.C) i32,
    set: ?*const fn (*device.Device, u32, u32, u8) callconv(.C) i32,
    configure: ?*const fn (*device.Device, u32, *const PwmConfig) callconv(.C) i32,
    get_config: ?*const fn (*device.Device, u32, *PwmConfig) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 辅助函数
// ════════════════════════════════════════════════════════════════════

pub fn enable(dev: *device.Device, channel: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.enable orelse return -6;
    return func(dev, channel);
}

pub fn disable(dev: *device.Device, channel: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.disable orelse return -6;
    return func(dev, channel);
}

/// 设置 PWM: 频率 (Hz) 和占空比 (百分比 0-100)
pub fn set(dev: *device.Device, channel: u32, freq_hz: u32, duty_percent: u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set orelse return -6;
    return func(dev, channel, freq_hz, duty_percent);
}

pub fn configure(dev: *device.Device, channel: u32, config: *const PwmConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure orelse return -6;
    return func(dev, channel, config);
}

fn getApi(dev: *device.Device) ?*const PwmApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
