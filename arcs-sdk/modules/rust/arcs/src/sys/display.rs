//! FFI binding for drivers/lisa_display/lisa_display.h
//!
//! Subset: get_capabilities / write / blanking_on / blanking_off /
//! set_brightness. Bus+panel attach (`attach_bus`, a large config struct with
//! unions and several device handles) is board-specific and done in C
//! (`main.c`); it is declared in the vtable for correct field offsets but not
//! wrapped here. All ops are blocking.
//!
//! Hand-written **subset** — intentionally NOT routed through bindgen-drift
//! (the full `lisa_display_config_t` bus/panel config with its unions is left
//! unbound; bus attach is done in C).

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// lisa_display_pixel_format_t (C enum = 4-byte int → u32)
pub const PIXEL_FORMAT_RGB_888: u32 = 0;
pub const PIXEL_FORMAT_RGB_565: u32 = 1;
pub const PIXEL_FORMAT_BGR_565: u32 = 2;

#[repr(C)]
pub struct DisplayCapabilities {
    pub width: u16,
    pub height: u16,
    pub pixel_format: u32,            // lisa_display_pixel_format_t
    pub orientation: u32,             // lisa_display_orientation_t
    pub supported_pixel_formats: u32, // bitmask
}

#[repr(C)]
pub struct DisplayBufferDesc {
    pub width: u16,
    pub height: u16,
    pub pitch: u16,
    pub buf_size: u32,
}

// ── api vtable (mirrors lisa_display_api_t) ──────────────────────────
#[repr(C)]
pub struct DisplayApi {
    pub get_capabilities:
        Option<unsafe extern "C" fn(*mut LisaDevice, *mut DisplayCapabilities) -> c_int>,
    pub write: Option<
        unsafe extern "C" fn(
            *mut LisaDevice,
            u16,
            u16,
            *const DisplayBufferDesc,
            *const c_void,
        ) -> c_int,
    >,
    pub blanking_on: Option<unsafe extern "C" fn(*mut LisaDevice) -> c_int>,
    pub blanking_off: Option<unsafe extern "C" fn(*mut LisaDevice) -> c_int>,
    pub set_brightness: Option<unsafe extern "C" fn(*mut LisaDevice, u8) -> c_int>,
    pub set_orientation: Option<unsafe extern "C" fn(*mut LisaDevice, u32) -> c_int>,
    // attach_bus takes `const lisa_display_config_t *` (done in C); typed as an
    // opaque pointer here so the vtable layout stays correct.
    pub attach_bus: Option<unsafe extern "C" fn(*const LisaDevice, *const c_void) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid display device; `caps` points to a writable `DisplayCapabilities`.
#[inline]
pub unsafe fn get_capabilities(dev: *mut LisaDevice, caps: *mut DisplayCapabilities) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const DisplayApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).get_capabilities } {
        Some(f) => unsafe { f(dev, caps) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid display device; `desc` describes `buf`, which points to
/// `desc.buf_size` readable bytes (RGB565 pixels).
#[inline]
pub unsafe fn write(
    dev: *mut LisaDevice,
    x: u16,
    y: u16,
    desc: *const DisplayBufferDesc,
    buf: *const c_void,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const DisplayApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).write } {
        Some(f) => unsafe { f(dev, x, y, desc, buf) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid display device.
#[inline]
pub unsafe fn blanking_on(dev: *mut LisaDevice) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const DisplayApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).blanking_on } {
        Some(f) => unsafe { f(dev) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid display device.
#[inline]
pub unsafe fn blanking_off(dev: *mut LisaDevice) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const DisplayApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).blanking_off } {
        Some(f) => unsafe { f(dev) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid display device. `brightness` is 0..=100.
#[inline]
pub unsafe fn set_brightness(dev: *mut LisaDevice, brightness: u8) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const DisplayApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).set_brightness } {
        Some(f) => unsafe { f(dev, brightness) },
        None => -6,
    }
}
