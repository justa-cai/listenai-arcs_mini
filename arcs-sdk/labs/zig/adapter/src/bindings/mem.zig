///! ARCS SDK 内存管理 FFI 绑定
///! 映射 lisa_mem.h 中的所有函数
const std = @import("std");

// ════════════════════════════════════════════════════════════════════
// extern C 函数声明 (lisa_mem.h)
// ════════════════════════════════════════════════════════════════════

/// 通用内存分配 (默认使用 PSRAM)
pub extern fn lisa_mem_alloc(size: u32) callconv(.C) ?*anyopaque;

/// 对齐内存分配
pub extern fn lisa_mem_align_alloc(alignment: u32, size: u32) callconv(.C) ?*anyopaque;

/// 重新分配内存
pub extern fn lisa_mem_realloc(ptr: ?*anyopaque, size: u32) callconv(.C) ?*anyopaque;

/// 分配并清零
pub extern fn lisa_mem_calloc(count: u32, size: u32) callconv(.C) ?*anyopaque;

/// 释放内存
pub extern fn lisa_mem_free(ptr: ?*anyopaque) callconv(.C) void;

// ── SRAM 专用分配 (内部 704KB SRAM, 高性能) ──────────────────────

/// SRAM 内存分配
pub extern fn lisa_mem_sram_alloc(size: u32) callconv(.C) ?*anyopaque;

/// SRAM 分配并清零
pub extern fn lisa_mem_sram_calloc(count: u32, size: u32) callconv(.C) ?*anyopaque;

/// SRAM 重新分配
pub extern fn lisa_mem_sram_realloc(ptr: ?*anyopaque, size: u32) callconv(.C) ?*anyopaque;

/// SRAM 释放
pub extern fn lisa_mem_sram_free(ptr: ?*anyopaque) callconv(.C) void;
