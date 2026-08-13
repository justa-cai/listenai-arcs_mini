///! ARCS SDK system heap FFI bindings.
///! Mirrors system/heap/sysheap.h.
pub extern fn sysheap_init() callconv(.C) void;

pub extern fn inram_malloc(alignment: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn inram_calloc(alignment: usize, num: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn inram_realloc(ptr: ?*anyopaque, size: usize) callconv(.C) ?*anyopaque;
pub extern fn inram_free(ptr: ?*anyopaque) callconv(.C) void;

pub extern fn exram_malloc(alignment: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn exram_calloc(alignment: usize, num: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn exram_realloc(ptr: ?*anyopaque, size: usize) callconv(.C) ?*anyopaque;
pub extern fn exram_free(ptr: ?*anyopaque) callconv(.C) void;

pub extern fn psram_malloc(size: usize) callconv(.C) ?*anyopaque;
pub extern fn psram_malloc_align(alignment: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn psram_calloc(num: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn psram_calloc_align(alignment: usize, num: usize, size: usize) callconv(.C) ?*anyopaque;
pub extern fn psram_realloc(ptr: ?*anyopaque, size: usize) callconv(.C) ?*anyopaque;
pub extern fn psram_free(ptr: ?*anyopaque) callconv(.C) void;

pub const init = sysheap_init;

pub const inramMalloc = inram_malloc;
pub const inramCalloc = inram_calloc;
pub const inramRealloc = inram_realloc;
pub const inramFree = inram_free;

pub const exramMalloc = exram_malloc;
pub const exramCalloc = exram_calloc;
pub const exramRealloc = exram_realloc;
pub const exramFree = exram_free;

pub const psramMalloc = psram_malloc;
pub const psramMallocAlign = psram_malloc_align;
pub const psramCalloc = psram_calloc;
pub const psramCallocAlign = psram_calloc_align;
pub const psramRealloc = psram_realloc;
pub const psramFree = psram_free;
