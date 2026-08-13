///! ARCS SDK Console 子系统 FFI 绑定
///! 映射 console.h

// ════════════════════════════════════════════════════════════════════
// Console 后端结构体
// ════════════════════════════════════════════════════════════════════

pub const ConsoleWriteFn = *const fn ([*]const u8, i32) callconv(.C) i32;
pub const ConsoleReadFn = *const fn ([*]u8, i32) callconv(.C) i32;

pub const ConsoleBackend = extern struct {
    write: ?ConsoleWriteFn,
    read: ?ConsoleReadFn,
};

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

pub extern fn console_init() callconv(.C) void;
pub extern fn console_backend_register(backend: *const ConsoleBackend) callconv(.C) void;
pub extern fn console_write(data: [*]const u8, len: i32) callconv(.C) i32;
pub extern fn console_read(data: [*]u8, len: i32) callconv(.C) i32;

// ── printk 系列 ─────────────────────────────────────────────────
pub extern fn printk(fmt: [*:0]const u8, ...) callconv(.C) i32;
