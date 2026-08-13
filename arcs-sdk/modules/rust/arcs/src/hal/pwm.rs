//! Safe wrapper over `sys::pwm` + embedded-hal `SetDutyCycle` impl.
//!
//! `Pwm` binds one channel of a PWM controller. All ops are blocking. Typical
//! use: `configure` (optional) → `set_duty_percent` → `enable`.

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;

pub use sys::pwm::{MODE_CENTER_ALIGNED, MODE_EDGE_ALIGNED, POLARITY_INVERTED, POLARITY_NORMAL};

/// Default carrier frequency (Hz) used until `set_frequency` is called.
pub const DEFAULT_FREQ_HZ: u32 = 1_000;

/// Handle to one channel of a PWM controller (e.g. `"pwm0"`, channel 0).
#[derive(Clone, Copy)]
pub struct Pwm {
    dev: NonNull<sys::device::LisaDevice>,
    channel: u32,
    frequency_hz: u32,
}

unsafe impl Send for Pwm {}
unsafe impl Sync for Pwm {}

impl Pwm {
    /// Open a PWM controller channel by device name (e.g. `c"pwm0"`).
    pub fn open(name: &CStr, channel: u32) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        let dev = NonNull::new(p).ok_or(Error::NotFound)?;
        Ok(Self {
            dev,
            channel,
            frequency_hz: DEFAULT_FREQ_HZ,
        })
    }

    /// The channel index this handle drives.
    pub fn channel(&self) -> u32 {
        self.channel
    }

    /// Set the carrier frequency (Hz) used by subsequent `set_duty_percent`.
    pub fn set_frequency(&mut self, hz: u32) {
        self.frequency_hz = hz;
    }

    /// Configure polarity + alignment mode (optional; sensible defaults apply).
    pub fn configure(&self, polarity: u32, mode: u32) -> Result<()> {
        let c = sys::pwm::PwmConfig { polarity, mode };
        Error::from_c(unsafe { sys::pwm::configure(self.dev.as_ptr(), self.channel, &c) })
    }

    /// Set duty cycle (0..=100 %) at the current frequency. Output is produced
    /// once `enable` is called.
    pub fn set_duty_percent(&self, duty: u8) -> Result<()> {
        if duty > 100 {
            return Err(Error::InvalidArg);
        }
        Error::from_c(unsafe {
            sys::pwm::set(self.dev.as_ptr(), self.channel, self.frequency_hz, duty)
        })
    }

    /// Start PWM output on this channel.
    pub fn enable(&self) -> Result<()> {
        Error::from_c(unsafe { sys::pwm::enable(self.dev.as_ptr(), self.channel) })
    }

    /// Stop PWM output on this channel.
    pub fn disable(&self) -> Result<()> {
        Error::from_c(unsafe { sys::pwm::disable(self.dev.as_ptr(), self.channel) })
    }
}

#[cfg(feature = "embedded-hal")]
mod eh_impls {
    use super::Pwm;
    use embedded_hal::pwm::{ErrorType, SetDutyCycle};

    impl ErrorType for Pwm {
        type Error = crate::Error;
    }

    impl SetDutyCycle for Pwm {
        fn max_duty_cycle(&self) -> u16 {
            // The lisa API works in whole percent.
            100
        }

        fn set_duty_cycle(&mut self, duty: u16) -> Result<(), Self::Error> {
            // embedded-hal guarantees `duty <= max_duty_cycle()` (= 100), so the
            // `min` is belt-and-braces before the u8 cast.
            Pwm::set_duty_percent(self, duty.min(100) as u8)
        }
    }
}
