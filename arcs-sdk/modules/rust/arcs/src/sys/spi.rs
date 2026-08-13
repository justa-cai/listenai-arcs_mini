//! FFI binding for drivers/lisa_spi/lisa_spi.h
//!
//! Master-mode subset: configure, transfer, write, read, register_callback.
//! The transfer/write/read C ops are asynchronous — completion is signalled
//! via the registered callback (fired from the SPI ISR). get_config is out
//! of scope.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── enum values ──────────────────────────────────────────────────────
// These mirror C `enum` types (`lisa_spi_mode_t`, `lisa_spi_bit_order_t`,
// `lisa_spi_transfer_flags_t`, `lisa_spi_transfer_mode_t`). A C enum is an
// `int` (4 bytes) in this ABI, so the corresponding `SpiConfig` fields are
// `u32` — NOT `u8`. Declaring them `u8` shrinks the struct and shifts every
// later field (notably `master_mode`), which the driver then reads as garbage.
pub const MODE_0: u32 = 0; // CPOL=0 CPHA=0
pub const MODE_1: u32 = 1;
pub const MODE_2: u32 = 2;
pub const MODE_3: u32 = 3;
pub const BIT_ORDER_MSB_FIRST: u32 = 0;
pub const BIT_ORDER_LSB_FIRST: u32 = 1;
pub const FLAG_SOFTWARE_CS: u32 = 0x00;
pub const FLAG_HARDWARE_CS: u32 = 0x01;
pub const DMA_TRANSFER: u32 = 0;
pub const INTERRUPT_TRANSFER: u32 = 1;

/// `lisa_spi_transfer_callback_t` — `void (*)(void *user_data)`.
pub type SpiCallback = unsafe extern "C" fn(*mut c_void);

#[repr(C)]
pub struct SpiConfig {
    pub frequency: u32,
    pub mode: u32,             // lisa_spi_mode_t (C enum = int)
    pub bit_order: u32,        // lisa_spi_bit_order_t (C enum = int)
    pub data_bits: u8,         // uint8_t
    pub flags: u32,            // lisa_spi_transfer_flags_t (C enum = int)
    pub master_mode: bool,     // bool
    pub tx_transfer_mode: u32, // lisa_spi_transfer_mode_t (C enum = int)
    pub tx_dma_channel: u8,    // uint8_t
    pub rx_transfer_mode: u32, // lisa_spi_transfer_mode_t (C enum = int)
    pub rx_dma_channel: u8,    // uint8_t
}

#[repr(C)]
pub struct SpiTransfer {
    pub tx_buf: *const u8,
    pub rx_buf: *mut u8,
    pub len: u32,
}

#[repr(C)]
pub struct SpiApi {
    pub configure: Option<unsafe extern "C" fn(*mut LisaDevice, *const SpiConfig) -> c_int>,
    pub get_config: Option<unsafe extern "C" fn(*mut LisaDevice, *mut SpiConfig) -> c_int>,
    pub transfer: Option<unsafe extern "C" fn(*mut LisaDevice, *const SpiTransfer) -> c_int>,
    pub write: Option<unsafe extern "C" fn(*mut LisaDevice, *const u8, u32) -> c_int>,
    pub read: Option<unsafe extern "C" fn(*mut LisaDevice, *mut u8, u32) -> c_int>,
    pub register_callback:
        Option<unsafe extern "C" fn(*mut LisaDevice, Option<SpiCallback>, *mut c_void) -> c_int>,
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
/// `dev` valid SPI device; `cfg` valid `SpiConfig` for the call.
#[inline]
pub unsafe fn configure(dev: *mut LisaDevice, cfg: *const SpiConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const SpiApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).configure } {
        Some(f) => unsafe { f(dev, cfg) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid SPI device; `xfer.tx_buf`/`rx_buf` point to `xfer.len` valid
/// bytes that must stay alive until the completion callback fires.
#[inline]
pub unsafe fn transfer(dev: *mut LisaDevice, xfer: *const SpiTransfer) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const SpiApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).transfer } {
        Some(f) => unsafe { f(dev, xfer) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid SPI device; `buf` points to `len` readable bytes alive until
/// completion.
#[inline]
pub unsafe fn write(dev: *mut LisaDevice, buf: *const u8, len: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const SpiApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).write } {
        Some(f) => unsafe { f(dev, buf, len) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid SPI device; `buf` points to `len` writable bytes alive until
/// completion.
#[inline]
pub unsafe fn read(dev: *mut LisaDevice, buf: *mut u8, len: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const SpiApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).read } {
        Some(f) => unsafe { f(dev, buf, len) },
        None => -6,
    }
}

/// # Safety
/// `dev` valid SPI device; `cb`/`user_data` must remain valid until a
/// subsequent re-register (e.g. unregistered in `Spi::drop`).
#[inline]
pub unsafe fn register_callback(
    dev: *mut LisaDevice,
    cb: Option<SpiCallback>,
    user_data: *mut c_void,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const SpiApi };
    if api.is_null() {
        return -1;
    }
    match unsafe { (*api).register_callback } {
        Some(f) => unsafe { f(dev, cb, user_data) },
        None => -6,
    }
}
