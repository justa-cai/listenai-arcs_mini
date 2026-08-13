///! ARCS SDK Zig 日志系统
///! 提供惯用 Zig 日志接口, 底层对接 lisa_log
///!
///! 用法:
///!   const log = arcs.log;
///!   log.info("Hello {s}, value={d}", .{"world", 42});
///!   log.err("Something failed: {}", .{err});
///!   log.debug("Debug info: {x}", .{ptr});
const std = @import("std");
const log_c = @import("../bindings/log.zig");
const mem_c = @import("../bindings/mem.zig");

// ════════════════════════════════════════════════════════════════════
// 日志级别
// ════════════════════════════════════════════════════════════════════
pub const Level = enum(u8) {
    none = 0,
    err = 1,
    warn = 2,
    info = 3,
    debug = 4,
    verbose = 5,
};

// ════════════════════════════════════════════════════════════════════
// 全局配置
// ════════════════════════════════════════════════════════════════════
var current_level: Level = .info;
var initialized: bool = false;

/// 初始化日志系统
pub fn init() !void {
    if (initialized) return;
    const ret = log_c.lisa_log_init();
    if (ret != 0) return error.LogInitFailed;
    initialized = true;
}

/// 设置全局日志级别
pub fn setLevel(level: Level) void {
    current_level = level;
    log_c.lisa_log_set_level(@intFromEnum(level));
}

/// 获取当前日志级别
pub fn getLevel() Level {
    return current_level;
}

// ════════════════════════════════════════════════════════════════════
// 格式化日志输出
// ════════════════════════════════════════════════════════════════════

/// 通用格式化日志函数
pub fn logFmt(level: Level, comptime tag: []const u8, comptime fmt: []const u8, args: anytype) void {
    if (@intFromEnum(level) > @intFromEnum(current_level)) return;

    const level_char: u8 = switch (level) {
        .err => 'E',
        .warn => 'W',
        .info => 'I',
        .debug => 'D',
        .verbose => 'V',
        .none => return,
    };

    // 使用栈上的固定缓冲区, 避免堆分配
    var buf: [512]u8 = undefined;
    const prefix = std.fmt.bufPrint(&buf, "[{c}][{s}] ", .{ level_char, tag }) catch return;
    const msg = std.fmt.bufPrint(buf[prefix.len..], fmt, args) catch return;
    const total = buf[0 .. prefix.len + msg.len];

    // 输出到 C 日志后端
    writeToBackend(total);
}

/// 底层 C 输出函数
extern fn console_write(data: [*]const u8, len: i32) callconv(.C) i32;
extern fn printk(fmt: [*:0]const u8, ...) callconv(.C) i32;

fn writeToBackend(msg: []const u8) void {
    if (msg.len == 0) return;

    if (initialized) {
        // 日志系统已初始化: 通过 console_write 输出 (线程安全, 带互斥锁)
        _ = console_write(msg.ptr, @intCast(msg.len));
        // 追加换行
        _ = console_write("\r\n", 2);
    } else {
        // 日志系统未初始化: 直接通过 printk 回退输出
        // printk 是 variadic 函数, 这里用 %.*s 输出定长字符串
        _ = printk("%.*s\r\n", @as(i32, @intCast(msg.len)), msg.ptr);
    }
}


// ════════════════════════════════════════════════════════════════════
// 便捷日志宏 (编译时字符串格式化)
// ════════════════════════════════════════════════════════════════════

/// 错误日志
pub fn err(comptime fmt: []const u8, args: anytype) void {
    logFmt(.err, "zig", fmt, args);
}

/// 警告日志
pub fn warn(comptime fmt: []const u8, args: anytype) void {
    logFmt(.warn, "zig", fmt, args);
}

/// 信息日志
pub fn info(comptime fmt: []const u8, args: anytype) void {
    logFmt(.info, "zig", fmt, args);
}

/// 调试日志
pub fn debug(comptime fmt: []const u8, args: anytype) void {
    logFmt(.debug, "zig", fmt, args);
}

/// 详细日志
pub fn verbose(comptime fmt: []const u8, args: anytype) void {
    logFmt(.verbose, "zig", fmt, args);
}

// ════════════════════════════════════════════════════════════════════
// 带 Tag 的日志器 (模拟 C 中的 LOG_TAG)
// ════════════════════════════════════════════════════════════════════

/// 创建带模块标签的日志器
pub fn scoped(comptime tag: []const u8) type {
    return struct {
        pub fn err(comptime fmt: []const u8, args: anytype) void {
            logFmt(.err, tag, fmt, args);
        }
        pub fn warn(comptime fmt: []const u8, args: anytype) void {
            logFmt(.warn, tag, fmt, args);
        }
        pub fn info(comptime fmt: []const u8, args: anytype) void {
            logFmt(.info, tag, fmt, args);
        }
        pub fn debug(comptime fmt: []const u8, args: anytype) void {
            logFmt(.debug, tag, fmt, args);
        }
        pub fn verbose(comptime fmt: []const u8, args: anytype) void {
            logFmt(.verbose, tag, fmt, args);
        }
    };
}

// ════════════════════════════════════════════════════════════════════
// 后端管理
// ════════════════════════════════════════════════════════════════════

pub const Backend = struct {
    pub fn add(name: [:0]const u8, output: log_c.LogOutputFn, data: ?*anyopaque) !void {
        const ret = log_c.lisa_log_backend_add(name.ptr, output, data);
        if (ret != 0) return error.BackendAddFailed;
    }

    pub fn remove(name: [:0]const u8) !void {
        const ret = log_c.lisa_log_backend_remove(name.ptr);
        if (ret != 0) return error.BackendRemoveFailed;
    }

    pub fn pauseAll() !void {
        const ret = log_c.lisa_log_backend_pause_all();
        if (ret != 0) return error.BackendPauseFailed;
    }

    pub fn resumeAll() !void {
        const ret = log_c.lisa_log_backend_resume_all();
        if (ret != 0) return error.BackendResumeFailed;
    }
};
