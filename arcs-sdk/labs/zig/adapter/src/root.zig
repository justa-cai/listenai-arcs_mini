///! ARCS SDK Zig Adapter
///! 为聆思 ARCS 嵌入式 SDK 提供完整的 Zig 语言支持
///!
///! ## 架构概览
///!
///! ```
///! ┌─────────────────────────────────────────────────────────┐
///! │                 Zig Application                         │
///! ├─────────────────────────────────────────────────────────┤
///! │   HAL Layer (惯用 Zig API)                              │
///! │   Gpio / Uart / I2c / Spi / Adc / Pwm / Flash          │
///! │   Display / Audio / Bluetooth / WiFi / Rtc              │
///! │   Thread / Mutex / Semaphore / Channel / Timer          │
///! │   Allocator (psram / sram) / Log / RingBuffer           │
///! ├─────────────────────────────────────────────────────────┤
///! │   Bindings Layer (C FFI 1:1 映射, SDK 模块)             │
///! ├─────────────────────────────────────────────────────────┤
///! │   ARCS SDK (C) — FreeRTOS / Drivers / Components        │
///! └─────────────────────────────────────────────────────────┘
///! ```
///!
///! ## 快速开始
///!
///! ```zig
///! const arcs = @import("arcs");
///!
///! pub fn main() !void {
///!     try arcs.log.init();
///!     arcs.log.info("Hello from Zig on ARCS!", .{});
///!
///!     var gpio = try arcs.Gpio.open("gpioa");
///!     try gpio.configOutput(5, .{ .init_high = true });
///!
///!     const alloc = arcs.allocator.psram();
///!     var list = std.ArrayList(u8).init(alloc);
///!     defer list.deinit();
///! }
///! ```

// ════════════════════════════════════════════════════════════════════
// Raw C Bindings (低层 FFI, 直接对应 C API)
// ════════════════════════════════════════════════════════════════════
pub const c = struct {
    pub const types = @import("bindings/c_types.zig");
    pub const mem = @import("bindings/mem.zig");
    pub const sysheap = @import("bindings/sysheap.zig");
    pub const thread = @import("bindings/thread.zig");
    pub const sync = @import("bindings/sync.zig");
    pub const timer = @import("bindings/timer.zig");
    pub const device = @import("bindings/device.zig");
    pub const gpio = @import("bindings/gpio.zig");
    pub const uart = @import("bindings/uart.zig");
    pub const i2c = @import("bindings/i2c.zig");
    pub const spi = @import("bindings/spi.zig");
    pub const adc = @import("bindings/adc.zig");
    pub const pwm = @import("bindings/pwm.zig");
    pub const log_raw = @import("bindings/log.zig");
    pub const flash = @import("bindings/flash.zig");
    pub const display = @import("bindings/display.zig");
    pub const lvgl = @import("bindings/lvgl.zig");
    pub const camera = @import("bindings/camera.zig");
    pub const rtc = @import("bindings/rtc.zig");
    pub const audio = @import("bindings/audio.zig");
    pub const bluetooth = @import("bindings/bluetooth.zig");
    pub const wifi = @import("bindings/wifi.zig");
    pub const console = @import("bindings/console.zig");
    pub const ringbuf = @import("bindings/ringbuf.zig");
    pub const iomux = @import("bindings/iomux.zig");
    pub const httpclient = @import("bindings/httpclient.zig");
    pub const kv = @import("bindings/kv.zig");
    pub const app_player = @import("bindings/app_player.zig");
};

// ════════════════════════════════════════════════════════════════════
// 高级 HAL API (惯用 Zig 接口)
// ════════════════════════════════════════════════════════════════════

// ── 内存管理 ─────────────────────────────────────────────────────
pub const allocator = @import("hal/allocator.zig");

// ── 日志系统 ─────────────────────────────────────────────────────
pub const log = @import("hal/log.zig");

// ── RTOS 原语 ────────────────────────────────────────────────────
pub const Thread = @import("hal/thread.zig").Thread;
pub const Mutex = @import("hal/sync.zig").Mutex;
pub const Semaphore = @import("hal/sync.zig").Semaphore;
pub const Channel = @import("hal/sync.zig").Channel;
pub const Timer = @import("hal/timer.zig").Timer;

// ── 设备框架 ─────────────────────────────────────────────────────
pub const Device = @import("hal/device.zig");

// ── 外设驱动 ─────────────────────────────────────────────────────
pub const Gpio = @import("hal/gpio.zig").Gpio;
pub const Uart = @import("hal/uart.zig").Uart;
pub const I2c = @import("hal/i2c.zig").I2c;
pub const Spi = @import("hal/spi.zig").Spi;
pub const Adc = @import("hal/adc.zig").Adc;
pub const Pwm = @import("hal/pwm.zig").Pwm;
pub const Flash = @import("hal/flash.zig").Flash;
pub const Display = @import("hal/display.zig").Display;
pub const Rtc = @import("hal/rtc.zig").Rtc;
pub const Audio = @import("hal/audio.zig").Audio;

// ── 连接组件 ─────────────────────────────────────────────────────
pub const Bluetooth = @import("hal/bluetooth.zig").Bluetooth;
pub const WiFi = @import("hal/wifi.zig").WiFi;

// ── 工具 ─────────────────────────────────────────────────────────
pub const RingBuffer = @import("hal/ringbuf.zig").RingBuffer;

// ── 类型 ─────────────────────────────────────────────────────────
pub const Priority = @import("bindings/c_types.zig").Priority;
pub const LogLevel = @import("bindings/c_types.zig").LogLevel;
pub const DeviceError = @import("bindings/c_types.zig").DeviceError;

// ════════════════════════════════════════════════════════════════════
// 便捷函数
// ════════════════════════════════════════════════════════════════════

/// 休眠指定毫秒 (让出 CPU)
pub fn sleep(ms: u32) void {
    Thread.sleep(ms);
}

/// 让出 CPU 时间片
pub fn yield() void {
    Thread.yield();
}

// ════════════════════════════════════════════════════════════════════
// 编译时验证 (确保所有模块可编译)
// ════════════════════════════════════════════════════════════════════
test {
    // Bindings layer
    _ = @import("bindings/c_types.zig");
    _ = @import("bindings/mem.zig");
    _ = @import("bindings/sysheap.zig");
    _ = @import("bindings/thread.zig");
    _ = @import("bindings/sync.zig");
    _ = @import("bindings/timer.zig");
    _ = @import("bindings/device.zig");
    _ = @import("bindings/gpio.zig");
    _ = @import("bindings/uart.zig");
    _ = @import("bindings/i2c.zig");
    _ = @import("bindings/spi.zig");
    _ = @import("bindings/adc.zig");
    _ = @import("bindings/pwm.zig");
    _ = @import("bindings/log.zig");
    _ = @import("bindings/flash.zig");
    _ = @import("bindings/display.zig");
    _ = @import("bindings/lvgl.zig");
    _ = @import("bindings/camera.zig");
    _ = @import("bindings/rtc.zig");
    _ = @import("bindings/audio.zig");
    _ = @import("bindings/bluetooth.zig");
    _ = @import("bindings/wifi.zig");
    _ = @import("bindings/console.zig");
    _ = @import("bindings/ringbuf.zig");
    _ = @import("bindings/iomux.zig");
    _ = @import("bindings/httpclient.zig");
    _ = @import("bindings/kv.zig");
    _ = @import("bindings/app_player.zig");

    // HAL layer
    _ = @import("hal/allocator.zig");
    _ = @import("hal/log.zig");
    _ = @import("hal/sync.zig");
    _ = @import("hal/thread.zig");
    _ = @import("hal/timer.zig");
    _ = @import("hal/device.zig");
    _ = @import("hal/gpio.zig");
    _ = @import("hal/uart.zig");
    _ = @import("hal/i2c.zig");
    _ = @import("hal/spi.zig");
    _ = @import("hal/adc.zig");
    _ = @import("hal/pwm.zig");
    _ = @import("hal/flash.zig");
    _ = @import("hal/display.zig");
    _ = @import("hal/rtc.zig");
    _ = @import("hal/audio.zig");
    _ = @import("hal/bluetooth.zig");
    _ = @import("hal/wifi.zig");
    _ = @import("hal/ringbuf.zig");
}
