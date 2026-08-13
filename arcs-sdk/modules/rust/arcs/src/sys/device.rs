//! FFI binding for drivers/lisa_device/lisa_device.h
//!
//! Hand-written; CI bindgen-drift check enforces consistency.
//! Last synced against C header at commit: d84462a1

use core::ffi::{c_char, c_int};

/// Opaque struct mirroring `lisa_device_t`. We never inspect its fields from
/// Rust; the SDK API takes/returns `*mut lisa_device_t` pointers directly.
#[repr(C)]
pub struct LisaDevice {
    _opaque: [u8; 0],
}

extern "C" {
    /// Look up a registered device by name. Returns NULL if not found.
    /// The returned pointer is owned by the device registry, NOT the caller —
    /// do not free it.
    pub fn lisa_device_get(name: *const c_char) -> *mut LisaDevice;

    pub fn lisa_device_init() -> c_int;
}

// Error-code constants from lisa_device.h, exposed in Rust as `u8`-typed
// negative literals are awkward — keep i32 and cast at call site.
pub const ERR_INVALID: i32 = -1;
pub const ERR_NOT_FOUND: i32 = -2;
pub const ERR_NOT_SUPPORT: i32 = -6;
