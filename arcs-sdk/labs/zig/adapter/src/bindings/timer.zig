///! ARCS SDK 定时器 FFI 绑定
///! 映射 lisa_timer.h

// ════════════════════════════════════════════════════════════════════
// 枚举类型
// ════════════════════════════════════════════════════════════════════
pub const TimerStatus = enum(u32) {
    stop = 0,
    start = 1,
};

pub const TimerType = enum(u32) {
    once = 0,
    periodic = 1,
};

// ════════════════════════════════════════════════════════════════════
// 回调和句柄类型
// ════════════════════════════════════════════════════════════════════

/// 回调参数用 *anyopaque 避免与 Timer 的循环依赖
/// C: typedef void (*lisa_timercb_t)(struct lisa_timer *timer);
pub const TimerCallback = *const fn (*anyopaque) callconv(.C) void;

pub const TimerHandle = ?*anyopaque;

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════
pub extern fn lisa_timer_create(period_ms: u32, cb: TimerCallback, arg: ?*anyopaque) callconv(.C) TimerHandle;
pub extern fn lisa_timer_delete(timer: TimerHandle) callconv(.C) i32;
pub extern fn lisa_timer_start(timer: TimerHandle) callconv(.C) i32;
pub extern fn lisa_timer_stop(timer: TimerHandle) callconv(.C) i32;
pub extern fn lisa_timer_change_period(timer: TimerHandle, period_ms: u32) callconv(.C) i32;
pub extern fn lisa_timer_isactive(timer: TimerHandle) callconv(.C) bool;
