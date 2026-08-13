//! Hand-written FFI bindings to the ARCS SDK C API.
//!
//! Each submodule maps to one or more C headers, name-mangled per:
//!   lisa_xxx.h → sys/<xxx>.rs
//!
//! Bindings here are written by hand to keep the build tool-chain
//! dependency-free. CI runs `bindgen` against the same headers to detect
//! drift; see tools/rust/bindgen-drift.sh.

pub mod adc;
pub mod audio;
pub mod bt;
pub mod device;
pub mod display;
pub mod flash;
pub mod fs;
pub mod gpio;
pub mod i2c;
pub mod log;
pub mod mem;
pub mod pwm;
pub mod reset;
pub mod rtc;
pub mod spi;
pub mod sync;
pub mod thread;
pub mod uart;
pub mod wifi;
