//! Safe wrapper over `sys::uart` (sync subset for MVP).

use crate::{sys, Error, Result};
use core::ffi::CStr;
use core::ptr::NonNull;

#[derive(Clone, Copy)]
pub struct Uart {
    dev: NonNull<sys::device::LisaDevice>,
}

unsafe impl Send for Uart {}
unsafe impl Sync for Uart {}

#[derive(Clone, Copy)]
pub enum Parity {
    None,
    Odd,
    Even,
}

pub struct Config {
    pub baudrate: u32,
    pub data_bits: u8, // 5..=8
    pub stop_bits: u8, // 1 | 2 (1.5 not exposed)
    pub parity: Parity,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            baudrate: 115_200,
            data_bits: 8,
            stop_bits: 1,
            parity: Parity::None,
        }
    }
}

impl Uart {
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        NonNull::new(p)
            .map(|dev| Self { dev })
            .ok_or(Error::NotFound)
    }

    pub fn configure(&self, cfg: &Config) -> Result<()> {
        let c = sys::uart::UartConfig {
            baudrate: cfg.baudrate,
            data_bits: cfg.data_bits,
            stop_bits: match cfg.stop_bits {
                2 => sys::uart::STOP_BITS_2,
                _ => sys::uart::STOP_BITS_1,
            },
            parity: match cfg.parity {
                Parity::None => sys::uart::PARITY_NONE,
                Parity::Odd => sys::uart::PARITY_ODD,
                Parity::Even => sys::uart::PARITY_EVEN,
            },
            flow_ctrl: sys::uart::FLOW_CTRL_NONE,
            transfer_mode: sys::uart::TRANSFER_INTERRUPT,
            _pad: [0; 3],
            rx_buf_config: sys::uart::RxBufConfig {
                buffer_count: 0,
                buffer_size: 0,
            },
            dma_tx_channel: 0xFF,
            dma_rx_channel: 0xFF,
            _pad2: [0; 2],
        };
        Error::from_c(unsafe { sys::uart::configure(self.dev.as_ptr(), &c) })
    }

    pub fn write_all(&self, buf: &[u8]) -> Result<()> {
        let n = unsafe {
            sys::uart::write_sync(self.dev.as_ptr(), buf.as_ptr(), buf.len() as u32, u32::MAX)
        };
        if n < 0 {
            return Err(Error::from_negative(n));
        }
        if (n as usize) != buf.len() {
            return Err(Error::Io(n));
        }
        Ok(())
    }

    /// Block until at least one byte is read (or read_sync returns due to
    /// idle timeout / buffer flush). Returns the number of bytes read.
    ///
    /// Renamed from `read` to avoid clashing with the
    /// [`embedded_io::Read::read`] trait method (which is impl'd for `Uart`
    /// below and delegates here).
    pub fn read_bytes(&self, buf: &mut [u8]) -> Result<usize> {
        let n = unsafe {
            sys::uart::read_sync(
                self.dev.as_ptr(),
                buf.as_mut_ptr(),
                buf.len() as u32,
                u32::MAX,
            )
        };
        if n < 0 {
            return Err(Error::from_negative(n));
        }
        Ok(n as usize)
    }

    pub fn rx_enable(&self) -> Result<()> {
        Error::from_c(unsafe { sys::uart::rx_enable(self.dev.as_ptr()) })
    }
}

#[cfg(feature = "embedded-hal")]
mod eio {
    use super::Uart;
    use embedded_io::{ErrorType, Read, Write};

    impl ErrorType for Uart {
        type Error = crate::Error;
    }

    impl Write for Uart {
        fn write(&mut self, buf: &[u8]) -> Result<usize, Self::Error> {
            // arcs Uart::write_all writes the whole buffer or errors, so a
            // full-length return on success is correct for embedded-io.
            self.write_all(buf)?;
            Ok(buf.len())
        }
        fn flush(&mut self) -> Result<(), Self::Error> {
            // write_sync is already blocking/complete; nothing to flush.
            Ok(())
        }
    }

    impl Read for Uart {
        // `read_bytes` blocks until at least one byte is available, so a
        // returned `Ok(0)` means "empty input buffer", never EOF — a UART
        // never closes. Callers using `read_exact` will not see a spurious
        // EOF in practice.
        fn read(&mut self, buf: &mut [u8]) -> Result<usize, Self::Error> {
            self.read_bytes(buf)
        }
    }
}
