//! Safe wrapper over `sys::adc` (blocking single-shot reads).
//!
//! embedded-hal 1.0 has no blocking ADC trait, so this is an inherent API only.

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;

pub use sys::adc::{
    REF_EXTERNAL, REF_VDD_1V2, REF_VDD_3V6, REF_VDD_IO_AUTO, REF_VDD_IO_AUTO_MUL3, RESOLUTION_10BIT,
};

/// Handle to an ADC controller (e.g. `"adc0"`). Channels are selected per-call.
#[derive(Clone, Copy)]
pub struct Adc {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Adc {}
unsafe impl Sync for Adc {}

impl Adc {
    /// Open an ADC controller by device name (e.g. `c"adc0"`).
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    /// Configure a channel's reference voltage + resolution. Call before
    /// `read`; if skipped the driver applies a default (1.2 V, 10-bit).
    pub fn configure_channel(&self, channel: u32, reference: u32, resolution: u32) -> Result<()> {
        let c = sys::adc::AdcChannelConfig {
            reference,
            resolution,
        };
        Error::from_c(unsafe { sys::adc::channel_setup(self.dev.as_ptr(), channel, &c) })
    }

    /// Read one raw conversion (right-aligned; 0..=1023 for 10-bit).
    pub fn read(&self, channel: u32) -> Result<u16> {
        let mut value: u16 = 0;
        Error::from_c(unsafe { sys::adc::read(self.dev.as_ptr(), channel, &mut value) })?;
        Ok(value)
    }
}

/// Convert a raw count to millivolts given the reference voltage (mV) and ADC
/// bit width. Mirrors the C `LISA_ADC_RAW_TO_MV` macro.
pub fn raw_to_mv(raw: u16, reference_mv: u32, bits: u32) -> u32 {
    let max = (1u32 << bits) - 1;
    (raw as u32 * reference_mv) / max
}
