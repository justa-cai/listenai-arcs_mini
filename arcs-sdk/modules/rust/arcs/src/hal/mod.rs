//! High-level, safe wrappers around the `sys::*` FFI.
//!
//! Modules here use RAII and `arcs::Result<T>` to surface SDK errors in
//! idiomatic Rust. Anything beyond what's wrapped is still accessible via
//! `arcs::sys::*` for power users.

pub mod adc;
pub mod audio;
pub mod bt;
pub mod delay;
pub mod display;
pub mod flash;
pub mod fs;
pub mod gpio;
pub mod i2c;
pub mod log;
pub mod pwm;
pub mod reset;
pub mod rtc;
pub mod spi;
pub mod sync;
pub mod thread;
pub mod uart;
pub mod wifi;
