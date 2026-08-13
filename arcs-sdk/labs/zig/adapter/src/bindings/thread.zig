///! ARCS SDK 线程管理 FFI 绑定
///! 映射 lisa_thread.h
const std = @import("std");

// ════════════════════════════════════════════════════════════════════
// 不透明句柄类型
// ════════════════════════════════════════════════════════════════════
pub const ThreadHandle = ?*anyopaque;

// ════════════════════════════════════════════════════════════════════
// C 结构体映射
// ════════════════════════════════════════════════════════════════════

/// 线程入口函数类型
pub const ThreadEntryFn = *const fn (?*anyopaque) callconv(.C) void;

/// 线程属性
pub const ThreadAttr = extern struct {
    name: ?[*]const u8,
    stack_size: u32,
    priority: u32,
};

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

/// 创建线程
pub extern fn lisa_thread_create(
    attr: *const ThreadAttr,
    entry: ThreadEntryFn,
    arg: ?*anyopaque,
) callconv(.C) ThreadHandle;

/// 删除线程
pub extern fn lisa_thread_delete(thread: ThreadHandle) callconv(.C) i32;

/// 设置优先级
pub extern fn lisa_thread_set_priority(thread: ThreadHandle, priority: u8) callconv(.C) i32;

/// 延迟 (tick 为单位)
pub extern fn lisa_thread_delay(ticks: u32) callconv(.C) i32;

/// 延迟 (毫秒)
pub extern fn lisa_thread_mdelay(ms: u32) callconv(.C) i32;

/// 让出 CPU
pub extern fn lisa_thread_yield() callconv(.C) i32;

/// 获取当前线程名
pub extern fn lisa_thread_cur_thread_name() callconv(.C) ?[*]u8;
