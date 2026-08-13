///! ARCS SDK GPIO 驱动 FFI 绑定
///! 映射 lisa_gpio.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// GPIO 模式枚举
// ════════════════════════════════════════════════════════════════════
pub const GpioMode = enum(u32) {
    input = 0,
    output = 1,
};

pub const GpioIrqMode = enum(u32) {
    rising = 0x01,
    falling = 0x02,
    both = 0x03,
    level_high = 0x04,
    level_low = 0x08,
};

// ════════════════════════════════════════════════════════════════════
// GPIO 配置标志 (位掩码)
// ════════════════════════════════════════════════════════════════════
pub const GPIO_INPUT: u32 = 0 << 0;
pub const GPIO_OUTPUT: u32 = 1 << 0;
pub const GPIO_PULL_UP: u32 = 1 << 1;
pub const GPIO_PULL_DOWN: u32 = 1 << 2;
pub const GPIO_DEBOUNCE: u32 = 1 << 3;
pub const GPIO_OUTPUT_INIT_LOW: u32 = 0 << 4;
pub const GPIO_OUTPUT_INIT_HIGH: u32 = 1 << 4;

pub const GPIO_LOW: u32 = 0;
pub const GPIO_HIGH: u32 = 1;

// ════════════════════════════════════════════════════════════════════
// GPIO 回调类型
// ════════════════════════════════════════════════════════════════════
pub const GpioIrqCallback = *const fn (u32, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// GPIO API 虚表 (vtable pattern)
// ════════════════════════════════════════════════════════════════════
pub const GpioApi = extern struct {
    configure: ?*const fn (*device.Device, u32, u32) callconv(.C) i32,
    get_config: ?*const fn (*device.Device, u32, *u32) callconv(.C) i32,
    read_pin: ?*const fn (*device.Device, u32) callconv(.C) i32,
    write_pin: ?*const fn (*device.Device, u32, u32) callconv(.C) i32,
    configure_irq: ?*const fn (*device.Device, u32, GpioIrqMode, GpioIrqCallback, ?*anyopaque) callconv(.C) i32,
    enable_irq: ?*const fn (*device.Device, u32) callconv(.C) i32,
    disable_irq: ?*const fn (*device.Device, u32) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联 API 辅助函数 (模拟 C 头文件中的 static inline)
// ════════════════════════════════════════════════════════════════════

pub fn configure(dev: *device.Device, pin: u32, flags: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure orelse return -6; // NOT_SUPPORT
    return func(dev, pin, flags);
}

pub fn readPin(dev: *device.Device, pin: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.read_pin orelse return -6;
    return func(dev, pin);
}

pub fn writePin(dev: *device.Device, pin: u32, value: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write_pin orelse return -6;
    return func(dev, pin, value);
}

pub fn configureIrq(dev: *device.Device, pin: u32, mode: GpioIrqMode, cb: GpioIrqCallback, user_data: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.configure_irq orelse return -6;
    return func(dev, pin, mode, cb, user_data);
}

pub fn enableIrq(dev: *device.Device, pin: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.enable_irq orelse return -6;
    return func(dev, pin);
}

pub fn disableIrq(dev: *device.Device, pin: u32) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.disable_irq orelse return -6;
    return func(dev, pin);
}

fn getApi(dev: *device.Device) ?*const GpioApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
