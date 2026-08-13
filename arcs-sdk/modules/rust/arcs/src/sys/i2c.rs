//! FFI binding for drivers/lisa_i2c/lisa_i2c.h
//!
//! Master-mode subset: configure, transfer, write, read. Slave mode and
//! get_config are out of scope for R2.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── transfer flags (lisa_i2c_flags_t) ────────────────────────────────
pub const FLAG_NONE: u8 = 0x00;
pub const FLAG_NO_START: u8 = 0x01;
pub const FLAG_NO_STOP: u8 = 0x02;
pub const FLAG_10BIT_ADDR: u8 = 0x04;
pub const FLAG_READ: u8 = 0x08;

#[repr(C)]
pub struct I2cConfig {
    pub speed: u32,
    pub master_mode: bool,
    pub slave_addr: u16,
}

#[repr(C)]
pub struct I2cMsg {
    pub addr: u16,
    pub flags: u8,
    pub len: u16,
    pub buf: *mut u8,
}

// ── api vtable (mirrors lisa_i2c_api_t) ──────────────────────────────
#[repr(C)]
pub struct I2cApi {
    pub configure: Option<unsafe extern "C" fn(*mut LisaDevice, *const I2cConfig) -> c_int>,
    pub get_config: Option<unsafe extern "C" fn(*mut LisaDevice, *mut I2cConfig) -> c_int>,
    pub transfer: Option<unsafe extern "C" fn(*mut LisaDevice, *mut I2cMsg, u32) -> c_int>,
    pub write: Option<unsafe extern "C" fn(*mut LisaDevice, u16, *const u8, u32) -> c_int>,
    pub read: Option<unsafe extern "C" fn(*mut LisaDevice, u16, *mut u8, u32) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` must be a valid I2C `*mut LisaDevice` from `lisa_device_get`; `cfg`
/// must point to an initialised `I2cConfig` valid for the call.
#[inline]
pub unsafe fn configure(dev: *mut LisaDevice, cfg: *const I2cConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const I2cApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).configure } {
        Some(f) => unsafe { f(dev, cfg) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid I2C device; `msgs` points to `num` initialised `I2cMsg`s whose
/// `buf`/`len` are valid for the call.
#[inline]
pub unsafe fn transfer(dev: *mut LisaDevice, msgs: *mut I2cMsg, num: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const I2cApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).transfer } {
        Some(f) => unsafe { f(dev, msgs, num) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid I2C device; `buf` points to `len` readable bytes.
#[inline]
pub unsafe fn write(dev: *mut LisaDevice, addr: u16, buf: *const u8, len: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const I2cApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).write } {
        Some(f) => unsafe { f(dev, addr, buf, len) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid I2C device; `buf` points to `len` writable bytes.
#[inline]
pub unsafe fn read(dev: *mut LisaDevice, addr: u16, buf: *mut u8, len: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const I2cApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).read } {
        Some(f) => unsafe { f(dev, addr, buf, len) },
        None => -6,
    }
}
