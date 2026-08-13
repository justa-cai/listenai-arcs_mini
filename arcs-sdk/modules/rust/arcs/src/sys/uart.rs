//! FFI binding for drivers/lisa_uart/lisa_uart.h
//!
//! MVP covers: configure, write_sync, read_sync, rx_enable, rx_disable.
//! Async API (CONFIG_LISA_UART_ASYNC_API) is out of scope.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.
//! Last synced against C header at commit: d84462a1

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── Enums (mirror lisa_uart_*_t) ─────────────────────────────────────
pub const DATA_BITS_5: u8 = 5;
pub const DATA_BITS_6: u8 = 6;
pub const DATA_BITS_7: u8 = 7;
pub const DATA_BITS_8: u8 = 8;

pub const STOP_BITS_1: u8 = 0;
pub const STOP_BITS_1_5: u8 = 1;
pub const STOP_BITS_2: u8 = 2;

pub const PARITY_NONE: u8 = 0;
pub const PARITY_ODD: u8 = 1;
pub const PARITY_EVEN: u8 = 2;

pub const FLOW_CTRL_NONE: u8 = 0;
pub const FLOW_CTRL_RTS_CTS: u8 = 1;
pub const FLOW_CTRL_XON_XOFF: u8 = 2;

pub const TRANSFER_INTERRUPT: u8 = 0;
pub const TRANSFER_DMA: u8 = 1;

// ── Config struct (mirrors lisa_uart_config_t) ───────────────────────
#[repr(C)]
pub struct RxBufConfig {
    pub buffer_count: u32,
    pub buffer_size: u32,
}

#[repr(C)]
pub struct UartConfig {
    pub baudrate: u32,
    pub data_bits: u8,
    pub stop_bits: u8,
    pub parity: u8,
    pub flow_ctrl: u8,
    pub transfer_mode: u8,
    pub _pad: [u8; 3], // align rx_buf_config to 4
    pub rx_buf_config: RxBufConfig,
    pub dma_tx_channel: u8,
    pub dma_rx_channel: u8,
    pub _pad2: [u8; 2],
}

// ── api vtable (mirrors lisa_uart_api_t, sync subset only) ───────────
#[repr(C)]
pub struct UartApi {
    pub configure: Option<unsafe extern "C" fn(*mut LisaDevice, *const UartConfig) -> c_int>,
    pub get_config: Option<unsafe extern "C" fn(*mut LisaDevice, *mut UartConfig) -> c_int>,
    pub write_sync: Option<unsafe extern "C" fn(*mut LisaDevice, *const u8, u32, u32) -> c_int>,
    pub read_sync: Option<unsafe extern "C" fn(*mut LisaDevice, *mut u8, u32, u32) -> c_int>,
    pub rx_enable: Option<unsafe extern "C" fn(*mut LisaDevice) -> c_int>,
    pub rx_disable: Option<unsafe extern "C" fn(*mut LisaDevice) -> c_int>,
    pub poll_in: Option<unsafe extern "C" fn(*mut LisaDevice, *mut u8) -> c_int>,
    pub poll_out: Option<unsafe extern "C" fn(*mut LisaDevice, u8)>,
    pub flush: Option<unsafe extern "C" fn(*mut LisaDevice) -> c_int>,
    // Async-only fn pointers omitted — not part of MVP.
}

extern "C" {
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered UART controller, and
/// `cfg` must point to a fully-initialised `UartConfig` valid for the
/// duration of the call. The device pointer is dereferenced internally
/// to dispatch through the vtable; NULL or stale pointers are undefined
/// behaviour.
#[inline]
pub unsafe fn configure(dev: *mut LisaDevice, cfg: *const UartConfig) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const UartApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).configure };
    match f {
        Some(fp) => unsafe { fp(dev, cfg) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered UART controller, and
/// `buf` must point to at least `len` initialised bytes valid for read
/// for the duration of the call. The device pointer is dereferenced
/// internally to dispatch through the vtable; NULL or stale pointers
/// are undefined behaviour.
#[inline]
pub unsafe fn write_sync(dev: *mut LisaDevice, buf: *const u8, len: u32, timeout_ms: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const UartApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).write_sync };
    match f {
        Some(fp) => unsafe { fp(dev, buf, len, timeout_ms) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered UART controller, and
/// `buf` must point to at least `len` bytes valid for write for the
/// duration of the call. The device pointer is dereferenced internally
/// to dispatch through the vtable; NULL or stale pointers are undefined
/// behaviour.
#[inline]
pub unsafe fn read_sync(dev: *mut LisaDevice, buf: *mut u8, len: u32, timeout_ms: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const UartApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).read_sync };
    match f {
        Some(fp) => unsafe { fp(dev, buf, len, timeout_ms) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered UART controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour.
#[inline]
pub unsafe fn rx_enable(dev: *mut LisaDevice) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const UartApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).rx_enable };
    match f {
        Some(fp) => unsafe { fp(dev) },
        None => -6,
    }
}
