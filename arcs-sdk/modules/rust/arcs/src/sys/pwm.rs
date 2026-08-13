//! FFI binding for drivers/lisa_pwm/lisa_pwm.h
//!
//! One PWM device (`pwm0`) with channel-addressed outputs (channel is a `u32`
//! argument, not a separate device). All ops are blocking; no completion
//! callback.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── enum values (C enum = 4-byte int → u32, NOT u8) ──────────────────
// lisa_pwm_mode_t
pub const MODE_EDGE_ALIGNED: u32 = 0;
pub const MODE_CENTER_ALIGNED: u32 = 1;
// lisa_pwm_polarity_t
pub const POLARITY_NORMAL: u32 = 0;
pub const POLARITY_INVERTED: u32 = 1;

#[repr(C)]
pub struct PwmConfig {
    pub polarity: u32, // lisa_pwm_polarity_t
    pub mode: u32,     // lisa_pwm_mode_t
}

// ── api vtable (mirrors lisa_pwm_api_t) ──────────────────────────────
#[repr(C)]
pub struct PwmApi {
    pub enable: Option<unsafe extern "C" fn(*mut LisaDevice, u32) -> c_int>,
    pub disable: Option<unsafe extern "C" fn(*mut LisaDevice, u32) -> c_int>,
    pub set: Option<unsafe extern "C" fn(*mut LisaDevice, u32, u32, u8) -> c_int>,
    pub configure: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *const PwmConfig) -> c_int>,
    pub get_config: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *mut PwmConfig) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` must be a valid PWM `*mut LisaDevice`.
#[inline]
pub unsafe fn configure(dev: *mut LisaDevice, channel: u32, cfg: *const PwmConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const PwmApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).configure } {
        Some(f) => unsafe { f(dev, channel, cfg) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid PWM device. `duty_percent` is 0..=100.
#[inline]
pub unsafe fn set(
    dev: *mut LisaDevice,
    channel: u32,
    frequency_hz: u32,
    duty_percent: u8,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const PwmApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).set } {
        Some(f) => unsafe { f(dev, channel, frequency_hz, duty_percent) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid PWM device.
#[inline]
pub unsafe fn enable(dev: *mut LisaDevice, channel: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const PwmApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).enable } {
        Some(f) => unsafe { f(dev, channel) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid PWM device.
#[inline]
pub unsafe fn disable(dev: *mut LisaDevice, channel: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const PwmApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).disable } {
        Some(f) => unsafe { f(dev, channel) },
        None => -6,
    }
}
