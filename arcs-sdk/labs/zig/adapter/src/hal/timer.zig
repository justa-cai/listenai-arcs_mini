///! ARCS SDK Zig 定时器封装
///!
///! 用法:
///!   // 一次性定时器
///!   var timer = try arcs.Timer.once(1000, myCallback);
///!   try timer.start();
///!
///!   // 周期定时器
///!   var timer = try arcs.Timer.periodic(500, myCallback);
///!   try timer.start();
///!   try timer.changePeriod(200);
const std = @import("std");
const c_timer = @import("../bindings/timer.zig");

pub const Timer = struct {
    handle: c_timer.TimerHandle,
    status: c_timer.TimerStatus,
    timer_type: c_timer.TimerType,
    period_ms: u32,
    cb: ?c_timer.TimerCallback,

    const Self = @This();

    /// 创建周期定时器
    pub fn periodic(period_ms: u32, callback: c_timer.TimerCallback) !Self {
        return create(period_ms, callback, .periodic);
    }

    /// 创建一次性定时器
    pub fn once(delay_ms: u32, callback: c_timer.TimerCallback) !Self {
        return create(delay_ms, callback, .once);
    }

    fn create(period_ms: u32, callback: c_timer.TimerCallback, timer_type: c_timer.TimerType) !Self {
        const handle = c_timer.lisa_timer_create(period_ms, callback, null);
        if (handle == null) return error.TimerCreateFailed;
        return Self{
            .handle = handle,
            .status = .stop,
            .timer_type = timer_type,
            .period_ms = period_ms,
            .cb = callback,
        };
    }

    pub fn deinit(self: *Self) void {
        _ = c_timer.lisa_timer_delete(self.handle);
    }

    pub fn start(self: *Self) !void {
        const ret = c_timer.lisa_timer_start(self.handle);
        if (ret != 0) return error.TimerStartFailed;
    }

    pub fn stop(self: *Self) !void {
        const ret = c_timer.lisa_timer_stop(self.handle);
        if (ret != 0) return error.TimerStopFailed;
    }

    pub fn changePeriod(self: *Self, new_period_ms: u32) !void {
        const ret = c_timer.lisa_timer_change_period(self.handle, new_period_ms);
        if (ret != 0) return error.TimerChangePeriodFailed;
    }

    pub fn isActive(self: *Self) bool {
        return c_timer.lisa_timer_isactive(self.handle);
    }
};
