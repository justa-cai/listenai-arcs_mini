//! ARCS SDK Rust adapter (no_std).
//!
//! See `modules/rust/README.md` for usage.
#![no_std]
#![deny(unsafe_op_in_unsafe_fn)]

extern crate alloc;

pub mod error;
pub mod hal;
pub mod heap;
pub mod sys;

#[doc(hidden)]
pub mod __private {
    pub use crate::private::*;
}
mod macros;
mod private; // macro_rules! entry — exported via #[macro_export]

pub use error::{Error, Result};
pub use heap::{RawBox, Zone};

// Re-export the `log` facade at `arcs::log` so users write `arcs::log::info!(...)`
// without having to also depend on the `log` crate.
pub use ::log;

pub use arcs_macros::main;

pub use hal::adc::{self, Adc};
pub use hal::audio::{self, Audio};
pub use hal::bt::{self, Bluetooth};
pub use hal::delay::Delay;
pub use hal::display::{self, Display};
pub use hal::flash::{self, Flash};
pub use hal::gpio::{Gpio, GpioPin, InputOpts, IrqMode, IrqRegistration, Level, OutputOpts};
pub use hal::i2c::{self, I2c};
pub use hal::pwm::{self, Pwm};
pub use hal::reset::reboot;
pub use hal::rtc::{self, DateTime, Rtc};
pub use hal::spi::{self, Spi};
pub use hal::sync::{Channel, Mutex, MutexGuard, Semaphore};
pub use hal::thread::Thread;
pub use hal::uart::{Config as UartConfig, Parity, Uart};
pub use hal::wifi::{self, ScanInfo, Wifi};
