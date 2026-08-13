//! Unified error type for the `arcs` crate.
//!
//! The ARCS SDK returns `lisa_err_t` (`int32_t`) from most functions: `0` =
//! success, negative codes match `LISA_DEVICE_ERR_*` in
//! `drivers/lisa_device/lisa_device.h`. `Error::from_c` maps those codes to
//! Rust variants; `Io(i32)` is the catch-all for unmapped codes so we never
//! silently drop information.

use core::ffi::c_int;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[non_exhaustive]
pub enum Error {
    NotFound,     // -2  LISA_DEVICE_ERR_NOT_FOUND
    Exists,       // -3  LISA_DEVICE_ERR_EXISTS
    NoMemory,     // -4  LISA_DEVICE_ERR_NO_MEM
    InitFail,     // -5
    NotSupported, // -6  LISA_DEVICE_ERR_NOT_SUPPORT
    Timeout,      // -7
    Busy,         // -8
    NotReady,     // -9
    Io(i32),      // LISA_DEVICE_ERR_IO (-10) and any unmapped negative
    // code; callers can distinguish via `Io(-10)` literal.
    Range,      // -11
    Overflow,   // -12
    Nack,       // -13
    InvalidArg, // -1  LISA_DEVICE_ERR_INVALID
}

pub type Result<T> = core::result::Result<T, Error>;

impl Error {
    /// Map a `lisa_err_t` return value to `Result<()>`.
    ///
    /// **Only use for C functions whose return value is 0 on success and a
    /// negative `LISA_DEVICE_ERR_*` code on failure.** For functions that
    /// return a positive count on success (e.g. bytes read), call the
    /// underlying FFI directly and check the sign yourself; do NOT route
    /// through this helper.
    pub(crate) fn from_c(code: c_int) -> Result<()> {
        if code == 0 {
            return Ok(());
        }
        debug_assert!(
            code < 0,
            "from_c called with positive value {}; use raw FFI for count-returning APIs",
            code
        );
        Err(Self::from_negative(code))
    }

    pub(crate) fn from_negative(code: c_int) -> Self {
        match code {
            -1 => Self::InvalidArg,
            -2 => Self::NotFound,
            -3 => Self::Exists,
            -4 => Self::NoMemory,
            -5 => Self::InitFail,
            -6 => Self::NotSupported,
            -7 => Self::Timeout,
            -8 => Self::Busy,
            -9 => Self::NotReady,
            -11 => Self::Range,
            -12 => Self::Overflow,
            -13 => Self::Nack,
            other => Self::Io(other),
        }
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_hal::digital::Error for Error {
    fn kind(&self) -> embedded_hal::digital::ErrorKind {
        // `digital::ErrorKind` is #[non_exhaustive] and exposes only `Other`
        // in embedded-hal 1.0 — so `Other` is the only valid mapping today.
        // The asymmetry with the richer `embedded_io::Error` mapping below is
        // intentional; do not "fix" it until eh adds specific digital kinds.
        embedded_hal::digital::ErrorKind::Other
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_io::Error for Error {
    fn kind(&self) -> embedded_io::ErrorKind {
        match self {
            Error::Timeout => embedded_io::ErrorKind::TimedOut,
            Error::InvalidArg => embedded_io::ErrorKind::InvalidInput,
            Error::NotFound => embedded_io::ErrorKind::NotFound,
            _ => embedded_io::ErrorKind::Other,
        }
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_hal::i2c::Error for Error {
    fn kind(&self) -> embedded_hal::i2c::ErrorKind {
        match self {
            Error::Nack => embedded_hal::i2c::ErrorKind::NoAcknowledge(
                embedded_hal::i2c::NoAcknowledgeSource::Unknown,
            ),
            _ => embedded_hal::i2c::ErrorKind::Other,
        }
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_hal::spi::Error for Error {
    fn kind(&self) -> embedded_hal::spi::ErrorKind {
        // `spi::ErrorKind` is #[non_exhaustive]; none of its specific variants
        // (Overrun/ModeFault/FrameFormat/ChipSelectFault) map cleanly onto the
        // generic lisa_err_t codes, so `Other` is the honest mapping.
        embedded_hal::spi::ErrorKind::Other
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_hal::pwm::Error for Error {
    fn kind(&self) -> embedded_hal::pwm::ErrorKind {
        // `pwm::ErrorKind` is #[non_exhaustive] and exposes only `Other` today.
        embedded_hal::pwm::ErrorKind::Other
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_storage::nor_flash::NorFlashError for Error {
    fn kind(&self) -> embedded_storage::nor_flash::NorFlashErrorKind {
        use embedded_storage::nor_flash::NorFlashErrorKind;
        match self {
            Error::Range | Error::Overflow => NorFlashErrorKind::OutOfBounds,
            _ => NorFlashErrorKind::Other,
        }
    }
}
