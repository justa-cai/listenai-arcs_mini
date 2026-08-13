//! Safe wrapper over `sys::spi` (master mode) + embedded-hal `SpiBus` impl.
//!
//! The lisa SPI `transfer`/`write`/`read` C ops are **asynchronous**: each one
//! kicks off the hardware transfer and returns immediately, signalling
//! completion via a callback fired from the SPI ISR (see `lisa_spi_arcs.c`
//! `spi_hal_event_callback`). This wrapper hides that: [`Spi::configure`]
//! registers an internal completion callback that releases a counting
//! [`Semaphore`], and every data op blocks on that semaphore afterwards —
//! presenting a synchronous, `embedded-hal`-compatible blocking API.

use crate::hal::sync::Semaphore;
use crate::{sys, Error, Result};
use core::ffi::{c_void, CStr};
use core::ptr::NonNull;
use core::time::Duration;

pub use sys::spi::{BIT_ORDER_LSB_FIRST, BIT_ORDER_MSB_FIRST, MODE_0, MODE_1, MODE_2, MODE_3};

/// Common master clock speeds (Hz). Any `u32` is accepted by [`Config`].
pub const SPEED_1MHZ: u32 = 1_000_000;
pub const SPEED_10MHZ: u32 = 10_000_000;

/// Default per-op completion timeout. A handful of bytes at 1 MHz completes in
/// tens of microseconds, so this is a generous upper bound that only trips on a
/// genuinely stuck bus.
const DEFAULT_TIMEOUT: Duration = Duration::from_millis(1000);

/// SPI master configuration.
///
/// `Default` mirrors the C `LISA_SPI_CONFIG_DEFAULT()`: 1 MHz, mode 0, 8-bit,
/// MSB-first, hardware CS, interrupt (non-DMA) transfer.
#[derive(Clone, Copy)]
pub struct Config {
    /// Clock frequency in Hz.
    pub frequency: u32,
    /// Clock polarity/phase: one of [`MODE_0`]..=[`MODE_3`].
    pub mode: u32,
    /// Bit order: [`BIT_ORDER_MSB_FIRST`] or [`BIT_ORDER_LSB_FIRST`].
    pub bit_order: u32,
    /// Data word width in bits (typically 8 or 16).
    pub data_bits: u8,
    /// `true` = driver-controlled hardware CS; `false` = caller drives CS.
    pub hardware_cs: bool,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            frequency: SPEED_1MHZ,
            mode: MODE_0,
            bit_order: BIT_ORDER_MSB_FIRST,
            data_bits: 8,
            hardware_cs: true,
        }
    }
}

/// Handle to an SPI controller (e.g. `"spi0"`/`"spi1"`) in master mode.
///
/// Unlike [`crate::I2c`], `Spi` is **not** `Copy`: it owns a completion
/// [`Semaphore`] whose heap-stable handle is handed to the SDK as the ISR
/// callback's `user_data`. Dropping `Spi` first unregisters that callback so a
/// late completion can never signal a freed semaphore.
///
/// Data methods take `&self` for ergonomic parity with `I2c`, but are **not**
/// reentrant: calling them concurrently on a shared `Spi` from multiple threads
/// would interleave transfers on the single completion semaphore. Wrap in a
/// [`crate::Mutex`] to share one across threads.
pub struct Spi {
    dev: NonNull<sys::device::LisaDevice>,
    done: Semaphore,
    timeout: Duration,
}

// SAFETY: the device pointer is owned by the SDK device registry and is stable
// for the program's lifetime; `Semaphore` is itself `Send`/`Sync`. See the
// reentrancy note above for the logical (non-memory-safety) caveat.
unsafe impl Send for Spi {}
unsafe impl Sync for Spi {}

/// ISR-context completion callback. `user_data` is the `LisaSemaphore*` passed
/// to `register_callback`; `lisa_semaphore_give` detects ISR context and uses
/// the FreeRTOS FromISR variant internally, so signalling here is safe.
unsafe extern "C" fn on_complete(user_data: *mut c_void) {
    if !user_data.is_null() {
        unsafe {
            let _ = sys::sync::lisa_semaphore_give(user_data as *mut sys::sync::LisaSemaphore);
        }
    }
}

impl Spi {
    /// Open an SPI controller by device name (e.g. `c"spi0"`).
    pub fn open(name: &CStr) -> Result<Self> {
        let p = unsafe { sys::device::lisa_device_get(name.as_ptr()) };
        let dev = NonNull::new(p).ok_or(Error::NotFound)?;
        // Counting semaphore: max 1, initial 0 (Semaphore::new maps to
        // xSemaphoreCreateCounting(1, 0)) — the completion-signal pattern.
        let done = Semaphore::new(1)?;
        Ok(Self {
            dev,
            done,
            timeout: DEFAULT_TIMEOUT,
        })
    }

    /// Override the per-op completion timeout (default 1 s).
    pub fn set_timeout(&mut self, timeout: Duration) {
        self.timeout = timeout;
    }

    /// Configure the controller as a master and register the internal
    /// completion callback. Call once before any transfer.
    pub fn configure(&self, cfg: &Config) -> Result<()> {
        let c = sys::spi::SpiConfig {
            frequency: cfg.frequency,
            mode: cfg.mode,
            bit_order: cfg.bit_order,
            data_bits: cfg.data_bits,
            flags: if cfg.hardware_cs {
                sys::spi::FLAG_HARDWARE_CS
            } else {
                sys::spi::FLAG_SOFTWARE_CS
            },
            master_mode: true,
            tx_transfer_mode: sys::spi::INTERRUPT_TRANSFER,
            tx_dma_channel: 0,
            rx_transfer_mode: sys::spi::INTERRUPT_TRANSFER,
            rx_dma_channel: 0,
        };
        Error::from_c(unsafe { sys::spi::configure(self.dev.as_ptr(), &c) })?;
        Error::from_c(unsafe {
            sys::spi::register_callback(
                self.dev.as_ptr(),
                Some(on_complete),
                self.done.raw_handle() as *mut c_void,
            )
        })
    }

    /// Drain any stale completion token (e.g. left by a previously timed-out
    /// op) so the next wait reflects only the op we are about to start.
    #[inline]
    fn arm(&self) {
        let _ = self.done.clear();
    }

    #[inline]
    fn wait_done(&self) -> Result<()> {
        self.done
            .acquire_timeout(self.timeout)
            .map_err(|_| Error::Timeout)
    }

    /// Full-duplex transfer. `tx` and `rx` must be the same non-zero length;
    /// byte *i* of `rx` is what arrived on MISO while byte *i* of `tx` went out
    /// on MOSI.
    pub fn transfer(&self, tx: &[u8], rx: &mut [u8]) -> Result<()> {
        if tx.is_empty() || tx.len() != rx.len() {
            return Err(Error::InvalidArg);
        }
        let xfer = sys::spi::SpiTransfer {
            tx_buf: tx.as_ptr(),
            rx_buf: rx.as_mut_ptr(),
            len: tx.len() as u32,
        };
        self.arm();
        Error::from_c(unsafe { sys::spi::transfer(self.dev.as_ptr(), &xfer) })?;
        self.wait_done()
    }

    /// Transmit only (received bytes discarded).
    pub fn write(&self, buf: &[u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        self.arm();
        Error::from_c(unsafe {
            sys::spi::write(self.dev.as_ptr(), buf.as_ptr(), buf.len() as u32)
        })?;
        self.wait_done()
    }

    /// Receive only; the controller clocks out dummy bytes to generate SCK.
    pub fn read(&self, buf: &mut [u8]) -> Result<()> {
        if buf.is_empty() {
            return Ok(());
        }
        self.arm();
        Error::from_c(unsafe {
            sys::spi::read(self.dev.as_ptr(), buf.as_mut_ptr(), buf.len() as u32)
        })?;
        self.wait_done()
    }
}

impl Drop for Spi {
    fn drop(&mut self) {
        // Unregister before `done` is dropped (and its handle freed), so a late
        // ISR completion cannot `give` a dangling semaphore. Struct fields drop
        // after this in declaration order (dev, done, timeout).
        unsafe {
            let _ = sys::spi::register_callback(self.dev.as_ptr(), None, core::ptr::null_mut());
        }
    }
}

#[cfg(feature = "embedded-hal")]
mod eh_impls {
    use super::Spi;
    use embedded_hal::spi::{ErrorType, SpiBus};

    impl ErrorType for Spi {
        type Error = crate::Error;
    }

    impl SpiBus<u8> for Spi {
        fn read(&mut self, words: &mut [u8]) -> Result<(), Self::Error> {
            Spi::read(self, words)
        }

        fn write(&mut self, words: &[u8]) -> Result<(), Self::Error> {
            Spi::write(self, words)
        }

        fn transfer(&mut self, read: &mut [u8], write: &[u8]) -> Result<(), Self::Error> {
            // Fast path: equal length maps straight onto the lisa full-duplex op.
            if read.len() == write.len() {
                return Spi::transfer(self, write, read);
            }
            // embedded-hal allows differing lengths: clock max(len) words,
            // padding the short `write` with 0x00 and discarding received bytes
            // past `read.len()`. lisa needs equal-length buffers, so stage
            // through heap temporaries.
            let n = read.len().max(write.len());
            if n == 0 {
                return Ok(());
            }
            let mut tx = alloc::vec![0u8; n];
            tx[..write.len()].copy_from_slice(write);
            let mut rx = alloc::vec![0u8; n];
            Spi::transfer(self, &tx, &mut rx)?;
            read.copy_from_slice(&rx[..read.len()]);
            Ok(())
        }

        fn transfer_in_place(&mut self, words: &mut [u8]) -> Result<(), Self::Error> {
            if words.is_empty() {
                return Ok(());
            }
            // lisa transfer needs distinct tx/rx buffers; stage the outgoing
            // bytes through a heap copy, receive back into `words`.
            let tx = words.to_vec();
            Spi::transfer(self, &tx, words)
        }

        fn flush(&mut self) -> Result<(), Self::Error> {
            // Every op already blocks until the ISR completion callback fires,
            // so there is nothing buffered to flush.
            Ok(())
        }
    }
}
