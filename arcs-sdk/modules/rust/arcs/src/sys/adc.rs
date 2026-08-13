//! FFI binding for drivers/lisa_adc/lisa_adc.h
//!
//! One ADC device (`adc0`) with channel-addressed reads (channel is a `u32`
//! argument). Reads are blocking (poll-based); no completion callback. The raw
//! count is returned via an out-param; the function return is the error code.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── enum values (C enum = 4-byte int → u32) ──────────────────────────
// lisa_adc_reference_t
pub const REF_VDD_1V2: u32 = 0;
pub const REF_VDD_3V6: u32 = 1;
pub const REF_VDD_IO_AUTO: u32 = 2;
pub const REF_VDD_IO_AUTO_MUL3: u32 = 3;
pub const REF_EXTERNAL: u32 = 4;
// lisa_adc_resolution_t
pub const RESOLUTION_10BIT: u32 = 10;

#[repr(C)]
pub struct AdcChannelConfig {
    pub reference: u32,  // lisa_adc_reference_t
    pub resolution: u32, // lisa_adc_resolution_t
}

// ── api vtable (mirrors lisa_adc_api_t) ──────────────────────────────
#[repr(C)]
pub struct AdcApi {
    pub read: Option<unsafe extern "C" fn(*mut LisaDevice, u32, *mut u16) -> c_int>,
    pub channel_setup:
        Option<unsafe extern "C" fn(*mut LisaDevice, u32, *const AdcChannelConfig) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid ADC device; `cfg` points to an initialised `AdcChannelConfig`.
#[inline]
pub unsafe fn channel_setup(
    dev: *mut LisaDevice,
    channel: u32,
    cfg: *const AdcChannelConfig,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AdcApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).channel_setup } {
        Some(f) => unsafe { f(dev, channel, cfg) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid ADC device; `value` points to a writable `u16` that receives
/// the raw conversion count.
#[inline]
pub unsafe fn read(dev: *mut LisaDevice, channel: u32, value: *mut u16) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const AdcApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).read } {
        Some(f) => unsafe { f(dev, channel, value) },
        None => -6,
    }
}
