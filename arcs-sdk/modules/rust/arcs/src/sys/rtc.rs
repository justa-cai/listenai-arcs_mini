//! FFI binding for drivers/lisa_rtc/lisa_rtc.h
//!
//! Subset: set_time / get_time. Alarm + periodic-interrupt + callback ops are
//! declared in the vtable (for correct field offsets) but not wrapped here.
//! `year` is an offset from 2000 (0..=127).
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── event bitmask (lisa_rtc_event_t; C enum = 4-byte int → u32) ──────
pub const EVENT_ALARM: u32 = 1 << 0;
pub const EVENT_SECOND: u32 = 1 << 1;
pub const EVENT_MINUTE: u32 = 1 << 2;
pub const EVENT_HOUR: u32 = 1 << 3;

/// `lisa_rtc_callback_t` — `void (*)(uint32_t event, void *user_data)`.
pub type RtcCallback = unsafe extern "C" fn(event: u32, user_data: *mut c_void);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RtcTime {
    pub year: u16, // offset from 2000 (0..=127)
    pub month: u8,
    pub day: u8,
    pub weekday: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
}

/// Identical layout to `RtcTime` (`lisa_rtc_alarm_t`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RtcAlarm {
    pub year: u16,
    pub month: u8,
    pub day: u8,
    pub weekday: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
}

#[repr(C)]
pub struct RtcCapabilities {
    pub has_alarm: bool,
    pub alarm_count: u8,
    pub min_year: u16,
    pub max_year: u16,
}

// ── api vtable (mirrors lisa_rtc_api_t) ──────────────────────────────
#[repr(C)]
pub struct RtcApi {
    pub set_time: Option<unsafe extern "C" fn(*mut LisaDevice, *const RtcTime) -> c_int>,
    pub get_time: Option<unsafe extern "C" fn(*mut LisaDevice, *mut RtcTime) -> c_int>,
    pub set_alarm: Option<unsafe extern "C" fn(*mut LisaDevice, u8, *const RtcAlarm) -> c_int>,
    pub get_alarm: Option<unsafe extern "C" fn(*mut LisaDevice, u8, *mut RtcAlarm) -> c_int>,
    pub enable_alarm: Option<unsafe extern "C" fn(*mut LisaDevice, u8, bool) -> c_int>,
    pub set_periodic_int: Option<unsafe extern "C" fn(*mut LisaDevice, u32, bool) -> c_int>,
    pub get_capabilities:
        Option<unsafe extern "C" fn(*mut LisaDevice, *mut RtcCapabilities) -> c_int>,
    pub set_callback:
        Option<unsafe extern "C" fn(*mut LisaDevice, Option<RtcCallback>, *mut c_void) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid RTC device; `time` points to an initialised `RtcTime`.
#[inline]
pub unsafe fn set_time(dev: *mut LisaDevice, time: *const RtcTime) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const RtcApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).set_time } {
        Some(f) => unsafe { f(dev, time) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid RTC device; `time` points to a writable `RtcTime`.
#[inline]
pub unsafe fn get_time(dev: *mut LisaDevice, time: *mut RtcTime) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const RtcApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).get_time } {
        Some(f) => unsafe { f(dev, time) },
        None => -6,
    }
}
