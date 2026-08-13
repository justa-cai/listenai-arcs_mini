//! FFI binding for drivers/lisa_flash/lisa_flash.h
//!
//! Subset: read / write / erase / page_layout (the last only to compute total
//! capacity). `sr_read`/`sr_write`/`get_parameters` are declared in the vtable
//! for correct field offsets but not wrapped. C `size_t` → Rust `usize`
//! (= u32 on riscv32imac); C `void*` → `*mut/*const c_void`.
//!
//! Writes require a prior erase; erase granularity is the sector size (4 KiB on
//! arcs) and writes are 4-byte aligned. Erased bytes read back as 0xFF.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

#[repr(C)]
pub struct FlashPagesLayout {
    pub pages_count: usize,
    pub pages_size: usize,
}

// ── api vtable (mirrors lisa_flash_api_t) ────────────────────────────
// `get_parameters` returns `const lisa_flash_parameters_t *` (a struct with a
// bitfield we don't need); typed here as an opaque `*const c_void` so the
// vtable layout stays correct without binding the bitfield struct.
#[repr(C)]
pub struct FlashApi {
    pub read: Option<unsafe extern "C" fn(*mut LisaDevice, usize, *mut c_void, usize) -> c_int>,
    pub write: Option<unsafe extern "C" fn(*mut LisaDevice, usize, *const c_void, usize) -> c_int>,
    pub erase: Option<unsafe extern "C" fn(*mut LisaDevice, usize, usize) -> c_int>,
    pub sr_read: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *mut u32) -> c_int>,
    pub sr_write: Option<unsafe extern "C" fn(*mut LisaDevice, u32, u32) -> c_int>,
    pub get_parameters: Option<unsafe extern "C" fn(*mut LisaDevice) -> *const c_void>,
    pub page_layout:
        Option<unsafe extern "C" fn(*mut LisaDevice, *mut usize) -> *const FlashPagesLayout>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid flash device; `data` points to `len` writable bytes.
#[inline]
pub unsafe fn read(dev: *mut LisaDevice, offset: usize, data: *mut c_void, len: usize) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const FlashApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).read } {
        Some(f) => unsafe { f(dev, offset, data, len) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid flash device; `data` points to `len` readable bytes; the target
/// region must already be erased.
#[inline]
pub unsafe fn write(dev: *mut LisaDevice, offset: usize, data: *const c_void, len: usize) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const FlashApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).write } {
        Some(f) => unsafe { f(dev, offset, data, len) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid flash device; `offset`/`size` should be sector-aligned.
#[inline]
pub unsafe fn erase(dev: *mut LisaDevice, offset: usize, size: usize) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const FlashApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).erase } {
        Some(f) => unsafe { f(dev, offset, size) },
        None => -6,
    }
}

/// Total device capacity in bytes, summed from the page layout. Returns 0 if
/// the layout is unavailable.
///
/// # Safety
/// `dev` valid flash device.
#[inline]
pub unsafe fn capacity(dev: *mut LisaDevice) -> usize {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const FlashApi };
    if api.is_null() {
        return 0;
    }
    let f = match unsafe { (*api).page_layout } {
        Some(f) => f,
        None => return 0,
    };
    let mut count: usize = 0;
    let layout = unsafe { f(dev, &mut count) };
    if layout.is_null() || count == 0 {
        return 0;
    }
    let mut total = 0usize;
    for i in 0..count {
        let seg = unsafe { &*layout.add(i) };
        total += seg.pages_count * seg.pages_size;
    }
    total
}
