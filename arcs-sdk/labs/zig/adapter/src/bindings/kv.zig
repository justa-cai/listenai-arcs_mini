//! ARCS SDK lisa_kv component FFI bindings.
//! Mirrors components/lisa_kv/lisa_kv.h.

const c = @cImport({
    @cInclude("stdint.h");
    @cInclude("stdbool.h");
    @cInclude("lisa_kv.h");
});

pub const c_api = c;

/// Owned NUL-terminated string returned by `lisa_kv_get_string`.
/// Release with `freeString`/`free`.
pub const StringValue = ?[*:0]u8;

/// Owned byte buffer returned by `lisa_kv_get_blob`.
/// Release with `freeBlob`/`free`; valid length is returned separately.
pub const BlobValue = ?[*]u8;

pub const init = c.lisa_kv_init;
pub const del = c.lisa_kv_del;
pub const free = c.lisa_kv_free;
pub const dump = c.lisa_kv_dump;
pub const setIntRaw = c.lisa_kv_set_int;
pub const getIntRaw = c.lisa_kv_get_int;
pub const setStringRaw = c.lisa_kv_set_string;
pub const getStringRaw = c.lisa_kv_get_string;
pub const setBoolRaw = c.lisa_kv_set_bool;
pub const getBoolRaw = c.lisa_kv_get_bool;
pub const clear = c.lisa_kv_clear;
pub const getBlobRaw = c.lisa_kv_get_blob;
pub const setBlobRaw = c.lisa_kv_set_blob;

pub inline fn setInt(key: [:0]const u8, value: c_int) c_int {
    return setIntRaw(key.ptr, value);
}

pub inline fn getInt(key: [:0]const u8, value: *c_int) c_int {
    return getIntRaw(key.ptr, value);
}

pub inline fn setString(key: [:0]const u8, value: [:0]const u8) c_int {
    return setStringRaw(key.ptr, value.ptr);
}

pub inline fn getString(key: [:0]const u8, value: *StringValue) c_int {
    return getStringRaw(key.ptr, @ptrCast(value));
}

pub inline fn freeString(value: StringValue) void {
    free(if (value) |ptr| @as(?*anyopaque, @ptrCast(ptr)) else null);
}

pub inline fn setBlob(key: [:0]const u8, data: []u8) c_int {
    return setBlobRaw(key.ptr, data.ptr, @intCast(data.len));
}

pub inline fn getBlob(key: [:0]const u8, value: *BlobValue, len: *c_int) c_int {
    return getBlobRaw(key.ptr, @ptrCast(value), len);
}

pub inline fn freeBlob(value: BlobValue) void {
    free(if (value) |ptr| @as(?*anyopaque, @ptrCast(ptr)) else null);
}

pub inline fn setBool(key: [:0]const u8, value: bool) c_int {
    return setBoolRaw(key.ptr, value);
}

pub inline fn getBool(key: [:0]const u8, value: *bool) c_int {
    return getBoolRaw(key.ptr, value);
}
