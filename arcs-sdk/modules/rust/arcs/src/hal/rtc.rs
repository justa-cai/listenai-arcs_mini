//! Safe wrapper over `sys::rtc` (set/get wall-clock time).
//!
//! embedded-hal 1.0 has no RTC trait, so this is an inherent API only.

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;

/// Wall-clock date/time. `year` is an offset from 2000 (0..=127), matching the
/// SDK — e.g. `year = 25` is 2025. `weekday` is 0..=6 (0 = Sunday).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DateTime {
    pub year: u16,
    pub month: u8,
    pub day: u8,
    pub weekday: u8,
    pub hour: u8,
    pub minute: u8,
    pub second: u8,
}

/// Handle to the RTC (e.g. `"rtc0"`).
#[derive(Clone, Copy)]
pub struct Rtc {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Rtc {}
unsafe impl Sync for Rtc {}

impl Rtc {
    /// Open the RTC by device name (e.g. `c"rtc0"`).
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    /// Set the current time.
    pub fn set(&self, t: &DateTime) -> Result<()> {
        let c = sys::rtc::RtcTime {
            year: t.year,
            month: t.month,
            day: t.day,
            weekday: t.weekday,
            hour: t.hour,
            minute: t.minute,
            second: t.second,
        };
        Error::from_c(unsafe { sys::rtc::set_time(self.dev.as_ptr(), &c) })
    }

    /// Read the current time.
    pub fn now(&self) -> Result<DateTime> {
        let mut c = sys::rtc::RtcTime {
            year: 0,
            month: 0,
            day: 0,
            weekday: 0,
            hour: 0,
            minute: 0,
            second: 0,
        };
        Error::from_c(unsafe { sys::rtc::get_time(self.dev.as_ptr(), &mut c) })?;
        Ok(DateTime {
            year: c.year,
            month: c.month,
            day: c.day,
            weekday: c.weekday,
            hour: c.hour,
            minute: c.minute,
            second: c.second,
        })
    }
}
