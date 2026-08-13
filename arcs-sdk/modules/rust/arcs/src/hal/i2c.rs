//! Safe wrapper over `sys::i2c` (master mode) + embedded-hal i2c impl.

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;

/// Standard/Fast/Fast+ bus speeds (Hz). Pass any u32 to `configure`.
pub const SPEED_STANDARD: u32 = 100_000;
pub const SPEED_FAST: u32 = 400_000;
pub const SPEED_FAST_PLUS: u32 = 1_000_000;

/// Handle to an I2C controller (e.g. "i2c0"). Owned by the SDK device
/// registry, so this is a `Copy` handle, not RAII (matches `Uart`/`Gpio`).
#[derive(Clone, Copy)]
pub struct I2c {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for I2c {}
unsafe impl Sync for I2c {}

impl I2c {
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    /// Configure as master at `speed` Hz.
    pub fn configure(&self, speed: u32) -> Result<()> {
        let c = sys::i2c::I2cConfig {
            speed,
            master_mode: true,
            slave_addr: 0,
        };
        Error::from_c(unsafe { sys::i2c::configure(self.dev.as_ptr(), &c) })
    }

    /// Blocking write to a 7-bit slave address.
    pub fn write(&self, addr: u16, buf: &[u8]) -> Result<()> {
        Error::from_c(unsafe {
            sys::i2c::write(self.dev.as_ptr(), addr, buf.as_ptr(), buf.len() as u32)
        })
    }

    /// Blocking read from a 7-bit slave address.
    pub fn read(&self, addr: u16, buf: &mut [u8]) -> Result<()> {
        Error::from_c(unsafe {
            sys::i2c::read(self.dev.as_ptr(), addr, buf.as_mut_ptr(), buf.len() as u32)
        })
    }

    /// Write then read in one transaction (repeated START between).
    pub fn write_read(&self, addr: u16, w: &[u8], r: &mut [u8]) -> Result<()> {
        let mut msgs = [
            sys::i2c::I2cMsg {
                addr,
                flags: sys::i2c::FLAG_NO_STOP,
                len: w.len() as u16,
                buf: w.as_ptr() as *mut u8,
            },
            sys::i2c::I2cMsg {
                addr,
                flags: sys::i2c::FLAG_READ,
                len: r.len() as u16,
                buf: r.as_mut_ptr(),
            },
        ];
        Error::from_c(unsafe {
            sys::i2c::transfer(self.dev.as_ptr(), msgs.as_mut_ptr(), msgs.len() as u32)
        })
    }

    /// 0-byte address probe. `Ok(true)` if the device ACKs, `Ok(false)` on
    /// NACK, `Err` on bus error. Used for bus scans.
    pub fn probe(&self, addr: u16) -> Result<bool> {
        let mut msg = sys::i2c::I2cMsg {
            addr,
            flags: sys::i2c::FLAG_NONE,
            len: 0,
            buf: core::ptr::null_mut(),
        };
        match Error::from_c(unsafe { sys::i2c::transfer(self.dev.as_ptr(), &mut msg, 1) }) {
            Ok(()) => Ok(true),
            Err(Error::Nack) => Ok(false),
            Err(e) => Err(e),
        }
    }
}

#[cfg(feature = "embedded-hal")]
mod eh_impls {
    use super::I2c;
    use crate::sys;
    use embedded_hal::i2c::{ErrorType, I2c as EhI2c, Operation, SevenBitAddress};

    impl ErrorType for I2c {
        type Error = crate::Error;
    }

    impl EhI2c<SevenBitAddress> for I2c {
        fn transaction(&mut self, addr: u8, ops: &mut [Operation<'_>]) -> Result<(), Self::Error> {
            // Build a lisa_i2c_msg_t[] from the embedded-hal operations.
            // STOP only after the final message (NO_STOP on all earlier ones),
            // which matches the embedded-hal transaction contract.
            let mut msgs: heapless::Vec<sys::i2c::I2cMsg, 8> = heapless::Vec::new();
            let n = ops.len();
            for (i, op) in ops.iter_mut().enumerate() {
                let last = i + 1 == n;
                let m = match op {
                    Operation::Read(buf) => sys::i2c::I2cMsg {
                        addr: addr as u16,
                        flags: sys::i2c::FLAG_READ | if last { 0 } else { sys::i2c::FLAG_NO_STOP },
                        len: buf.len() as u16,
                        buf: buf.as_mut_ptr(),
                    },
                    Operation::Write(buf) => sys::i2c::I2cMsg {
                        addr: addr as u16,
                        flags: if last { 0 } else { sys::i2c::FLAG_NO_STOP },
                        len: buf.len() as u16,
                        buf: buf.as_ptr() as *mut u8,
                    },
                };
                msgs.push(m).map_err(|_| crate::Error::InvalidArg)?;
            }
            if msgs.is_empty() {
                return Ok(());
            }
            crate::Error::from_c(unsafe {
                sys::i2c::transfer(self.dev.as_ptr(), msgs.as_mut_ptr(), msgs.len() as u32)
            })
        }
    }
}
