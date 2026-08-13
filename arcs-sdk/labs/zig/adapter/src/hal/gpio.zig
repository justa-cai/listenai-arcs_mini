///! ARCS SDK Zig GPIO 驱动封装
///! 提供类型安全、惯用 Zig 的 GPIO 接口
///!
///! 用法:
///!   var gpio = try arcs.Gpio.open("gpioa");
///!   try gpio.configOutput(5, .{ .pull = .up, .init_high = true });
///!   try gpio.write(5, .high);
///!   const level = try gpio.read(5);
///!
///!   // 中断
///!   try gpio.onInterrupt(3, .rising, myHandler, null);
///!   try gpio.enableInterrupt(3);
const std = @import("std");
const c_gpio = @import("../bindings/gpio.zig");
const c_device = @import("../bindings/device.zig");

pub const Gpio = struct {
    dev: *c_device.Device,

    const Self = @This();

    // ── 打开设备 ─────────────────────────────────────────────────

    /// 按名称打开 GPIO 设备
    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    // ── 电平值 ───────────────────────────────────────────────────

    pub const Level = enum(u32) {
        low = 0,
        high = 1,
    };

    pub const Pull = enum {
        none,
        up,
        down,
    };

    pub const OutputConfig = struct {
        pull: Pull = .none,
        init_high: bool = false,
        debounce: bool = false,
    };

    pub const InputConfig = struct {
        pull: Pull = .none,
        debounce: bool = false,
    };

    // ── 配置 ─────────────────────────────────────────────────────

    /// 配置引脚为输出模式
    pub fn configOutput(self: Self, pin: u32, config: OutputConfig) !void {
        var flags: u32 = c_gpio.GPIO_OUTPUT;
        switch (config.pull) {
            .up => flags |= c_gpio.GPIO_PULL_UP,
            .down => flags |= c_gpio.GPIO_PULL_DOWN,
            .none => {},
        }
        if (config.debounce) flags |= c_gpio.GPIO_DEBOUNCE;
        if (config.init_high) flags |= c_gpio.GPIO_OUTPUT_INIT_HIGH;

        const ret = c_gpio.configure(self.dev, pin, flags);
        if (ret != 0) return mapError(ret);
    }

    /// 配置引脚为输入模式
    pub fn configInput(self: Self, pin: u32, config: InputConfig) !void {
        var flags: u32 = c_gpio.GPIO_INPUT;
        switch (config.pull) {
            .up => flags |= c_gpio.GPIO_PULL_UP,
            .down => flags |= c_gpio.GPIO_PULL_DOWN,
            .none => {},
        }
        if (config.debounce) flags |= c_gpio.GPIO_DEBOUNCE;

        const ret = c_gpio.configure(self.dev, pin, flags);
        if (ret != 0) return mapError(ret);
    }

    // ── 读写 ─────────────────────────────────────────────────────

    /// 读取引脚电平
    pub fn read(self: Self, pin: u32) !Level {
        const ret = c_gpio.readPin(self.dev, pin);
        if (ret < 0) return mapError(ret);
        return if (ret == 0) .low else .high;
    }

    /// 写入引脚电平
    pub fn write(self: Self, pin: u32, level: Level) !void {
        const ret = c_gpio.writePin(self.dev, pin, @intFromEnum(level));
        if (ret != 0) return mapError(ret);
    }

    /// 翻转引脚电平
    pub fn toggle(self: Self, pin: u32) !void {
        const current = try self.read(pin);
        try self.write(pin, if (current == .low) .high else .low);
    }

    // ── 中断 ─────────────────────────────────────────────────────

    pub const IrqMode = c_gpio.GpioIrqMode;

    /// 配置引脚中断
    pub fn onInterrupt(self: Self, pin: u32, mode: IrqMode, callback: c_gpio.GpioIrqCallback, user_data: ?*anyopaque) !void {
        const ret = c_gpio.configureIrq(self.dev, pin, mode, callback, user_data);
        if (ret != 0) return mapError(ret);
    }

    /// 使能引脚中断
    pub fn enableInterrupt(self: Self, pin: u32) !void {
        const ret = c_gpio.enableIrq(self.dev, pin);
        if (ret != 0) return mapError(ret);
    }

    /// 禁止引脚中断
    pub fn disableInterrupt(self: Self, pin: u32) !void {
        const ret = c_gpio.disableIrq(self.dev, pin);
        if (ret != 0) return mapError(ret);
    }
};

// ════════════════════════════════════════════════════════════════════
// 错误映射
// ════════════════════════════════════════════════════════════════════
pub const GpioError = error{
    DeviceNotFound,
    DeviceNotReady,
    InvalidParam,
    NotSupported,
    Timeout,
    Busy,
    IoError,
    OutOfRange,
    Unknown,
};

fn mapError(code: i32) GpioError {
    return switch (code) {
        -1 => error.InvalidParam,
        -2 => error.DeviceNotFound,
        -6 => error.NotSupported,
        -7 => error.Timeout,
        -8 => error.Busy,
        -10 => error.IoError,
        -11 => error.OutOfRange,
        else => error.Unknown,
    };
}
