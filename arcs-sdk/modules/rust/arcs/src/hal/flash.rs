//! Safe wrapper over `sys::flash` + embedded-storage `NorFlash` impl.
//!
//! Blocking NOR-flash read/write/erase. Writes require a prior erase of the
//! target sector; erase granularity is [`ERASE_SIZE`] and writes are
//! [`WRITE_SIZE`]-aligned. Erased bytes read back as `0xFF`.

use crate::{sys, Error, Result};
use core::ffi::{c_void, CStr};
use core::ptr::NonNull;

/// Erase granularity (sector size) on arcs: 4 KiB.
pub const ERASE_SIZE: usize = 4096;
/// Minimum write alignment on arcs: 4 bytes.
pub const WRITE_SIZE: usize = 4;

/// Handle to a NOR-flash device (e.g. `"flash0"`).
#[derive(Clone, Copy)]
pub struct Flash {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Flash {}
unsafe impl Sync for Flash {}

impl Flash {
    /// Open a flash device by name (e.g. `c"flash0"`).
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    /// Read `buf.len()` bytes starting at `offset`.
    pub fn read(&self, offset: u32, buf: &mut [u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        Error::from_c(unsafe {
            sys::flash::read(
                self.dev.as_ptr(),
                offset as usize,
                buf.as_mut_ptr() as *mut c_void,
                buf.len(),
            )
        })
    }

    /// Write `buf` at `offset`. The target region must already be erased and
    /// `offset`/`buf.len()` should respect [`WRITE_SIZE`] alignment.
    pub fn write(&self, offset: u32, buf: &[u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        Error::from_c(unsafe {
            sys::flash::write(
                self.dev.as_ptr(),
                offset as usize,
                buf.as_ptr() as *const c_void,
                buf.len(),
            )
        })
    }

    /// Erase `len` bytes at `offset` (both should be [`ERASE_SIZE`]-aligned).
    pub fn erase(&self, offset: u32, len: u32) -> Result<()> {
        Error::from_c(unsafe {
            sys::flash::erase(self.dev.as_ptr(), offset as usize, len as usize)
        })
    }

    /// Total device capacity in bytes (0 if the layout is unavailable).
    pub fn capacity(&self) -> usize {
        unsafe { sys::flash::capacity(self.dev.as_ptr()) }
    }
}

#[cfg(feature = "embedded-hal")]
mod eh_impls {
    use super::Flash;
    use embedded_storage::nor_flash::{ErrorType, NorFlash, ReadNorFlash};

    impl ErrorType for Flash {
        type Error = crate::Error;
    }

    impl ReadNorFlash for Flash {
        const READ_SIZE: usize = 1;

        fn read(&mut self, offset: u32, bytes: &mut [u8]) -> Result<(), Self::Error> {
            Flash::read(self, offset, bytes)
        }

        fn capacity(&self) -> usize {
            Flash::capacity(self)
        }
    }

    impl NorFlash for Flash {
        const WRITE_SIZE: usize = super::WRITE_SIZE;
        const ERASE_SIZE: usize = super::ERASE_SIZE;

        fn erase(&mut self, from: u32, to: u32) -> Result<(), Self::Error> {
            // embedded-storage erase takes [from, to); lisa takes (offset, len).
            Flash::erase(self, from, to.saturating_sub(from))
        }

        fn write(&mut self, offset: u32, bytes: &[u8]) -> Result<(), Self::Error> {
            Flash::write(self, offset, bytes)
        }
    }
}
