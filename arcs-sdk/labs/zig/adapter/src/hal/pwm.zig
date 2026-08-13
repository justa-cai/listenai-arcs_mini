///! ARCS SDK Zig PWM 驱动封装
///!
///! 用法:
///!   var pwm = try arcs.Pwm.open("pwm0");
///!   try pwm.set(0, 1000, 50);  // 通道0, 1kHz, 50% 占空比
///!   try pwm.enable(0);
///!   // ...
///!   try pwm.disable(0);
const c_pwm = @import("../bindings/pwm.zig");
const c_device = @import("../bindings/device.zig");

pub const Pwm = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    pub const Polarity = c_pwm.PwmPolarity;

    /// 设置 PWM 参数
    pub fn set(self: Self, channel: u32, freq_hz: u32, duty_percent: u8) !void {
        const ret = c_pwm.set(self.dev, channel, freq_hz, duty_percent);
        if (ret != 0) return error.SetFailed;
    }

    /// 使能 PWM 通道
    pub fn enable(self: Self, channel: u32) !void {
        const ret = c_pwm.enable(self.dev, channel);
        if (ret != 0) return error.EnableFailed;
    }

    /// 禁止 PWM 通道
    pub fn disable(self: Self, channel: u32) !void {
        const ret = c_pwm.disable(self.dev, channel);
        if (ret != 0) return error.DisableFailed;
    }

    /// 配置极性
    pub fn setPolarity(self: Self, channel: u32, polarity: Polarity) !void {
        const config = c_pwm.PwmConfig{ .polarity = polarity };
        const ret = c_pwm.configure(self.dev, channel, &config);
        if (ret != 0) return error.ConfigureFailed;
    }
};
