///! ARCS SDK Zig 内存分配器
///! 将 lisa_mem 包装为标准 Zig std.mem.Allocator 接口
///!
///! 用法:
///!   const alloc = arcs.allocator.psram();   // PSRAM 分配器 (默认, 大容量)
///!   const alloc = arcs.allocator.sram();    // SRAM 分配器 (高速, 704KB)
///!
///!   var list = std.ArrayList(u8).init(alloc);
///!   try list.append(42);
const std = @import("std");
const mem_c = @import("../bindings/mem.zig");

// ════════════════════════════════════════════════════════════════════
// PSRAM 分配器 (默认, 外部大容量内存)
// ════════════════════════════════════════════════════════════════════
pub fn psram() std.mem.Allocator {
    return .{
        .ptr = undefined,
        .vtable = &psram_vtable,
    };
}

const psram_vtable = std.mem.Allocator.VTable{
    .alloc = psramAlloc,
    .resize = psramResize,
    .free = psramFree,
};

fn psramAlloc(_: *anyopaque, len: usize, ptr_align: u8, _: usize) ?[*]u8 {
    const alignment = @as(u32, 1) << @intCast(ptr_align);
    if (alignment <= 8) {
        // lisa_mem_alloc 默认至少 8 字节对齐
        const ptr = mem_c.lisa_mem_alloc(@intCast(len));
        return if (ptr) |p| @ptrCast(p) else null;
    } else {
        const ptr = mem_c.lisa_mem_align_alloc(alignment, @intCast(len));
        return if (ptr) |p| @ptrCast(p) else null;
    }
}

fn psramResize(_: *anyopaque, buf: []u8, _: u8, new_len: usize, _: usize) bool {
    // lisa_mem_realloc 可能移动指针, resize 要求不移动
    // 只有缩小时可以安全 resize (不移动指针)
    if (new_len <= buf.len) return true;
    // 无法原地扩大, 返回 false 让 Allocator 走 alloc+copy+free 路径
    return false;
}

fn psramFree(_: *anyopaque, buf: []u8, _: u8, _: usize) void {
    mem_c.lisa_mem_free(buf.ptr);
}

// ════════════════════════════════════════════════════════════════════
// SRAM 分配器 (内部高速 SRAM, 容量有限)
// ════════════════════════════════════════════════════════════════════
pub fn sram() std.mem.Allocator {
    return .{
        .ptr = undefined,
        .vtable = &sram_vtable,
    };
}

const sram_vtable = std.mem.Allocator.VTable{
    .alloc = sramAlloc,
    .resize = sramResize,
    .free = sramFree,
};

fn sramAlloc(_: *anyopaque, len: usize, _: u8, _: usize) ?[*]u8 {
    const ptr = mem_c.lisa_mem_sram_alloc(@intCast(len));
    return if (ptr) |p| @ptrCast(p) else null;
}

fn sramResize(_: *anyopaque, _: []u8, _: u8, _: usize, _: usize) bool {
    return false;
}

fn sramFree(_: *anyopaque, buf: []u8, _: u8, _: usize) void {
    mem_c.lisa_mem_sram_free(buf.ptr);
}

// ════════════════════════════════════════════════════════════════════
// 便捷函数: 直接分配/释放 (不通过 Allocator 接口)
// ════════════════════════════════════════════════════════════════════

/// 分配原始内存 (PSRAM)
pub fn rawAlloc(size: u32) ?*anyopaque {
    return mem_c.lisa_mem_alloc(size);
}

/// 分配对齐内存 (PSRAM)
pub fn rawAlignAlloc(alignment: u32, size: u32) ?*anyopaque {
    return mem_c.lisa_mem_align_alloc(alignment, size);
}

/// 释放内存
pub fn rawFree(ptr: ?*anyopaque) void {
    mem_c.lisa_mem_free(ptr);
}
