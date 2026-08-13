///! ARCS SDK 日志系统 FFI 绑定
///! 映射 lisa_log.h

// ════════════════════════════════════════════════════════════════════
// 日志回调类型
// ════════════════════════════════════════════════════════════════════
pub const LogOutputFn = *const fn ([*]const u8, u32, ?*anyopaque) callconv(.C) void;

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明
// ════════════════════════════════════════════════════════════════════

pub extern fn lisa_log_init() callconv(.C) i32;
pub extern fn lisa_log_set_level(level: u8) callconv(.C) void;
pub extern fn lisa_log_backend_add(name: ?[*]const u8, output: LogOutputFn, data: ?*anyopaque) callconv(.C) i32;
pub extern fn lisa_log_backend_remove(name: ?[*]const u8) callconv(.C) i32;
pub extern fn lisa_log_backend_resume_all() callconv(.C) i32;
pub extern fn lisa_log_backend_pause_all() callconv(.C) i32;
pub extern fn lisa_log_backend_resume(name: ?[*]const u8) callconv(.C) i32;
pub extern fn lisa_log_backend_pause(name: ?[*]const u8) callconv(.C) i32;

// 注意: LOGE/LOGW/LOGI/LOGD/LOGV 在 C 中是宏, 依赖 LOG_TAG 和 variadic 参数
// Zig 中无法直接使用, 需要通过高级封装实现
// 参见 hal/log.zig 中的 Zig 原生日志实现
