//! FFI binding for drivers/lisa_gpio/lisa_gpio.h
//!
//! All public APIs are `static inline` wrappers around an api-vtable in
//! `dev->api`. We bind the vtable struct and reimplement the inline
//! wrappers as Rust `unsafe fn`s. CONFIG_LISA_PM wakeup APIs are out of
//! scope for MVP.
//!
//! Hand-written; CI bindgen-drift check enforces consistency.
//! Last synced against C header at commit: d84462a1

use crate::sys::device::LisaDevice;
use core::ffi::{c_int, c_void};

// ── Flag constants ───────────────────────────────────────────────────
pub const FLAG_INPUT: u32 = 0;
pub const FLAG_OUTPUT: u32 = 1 << 0;
pub const FLAG_PULL_UP: u32 = 1 << 1;
pub const FLAG_PULL_DOWN: u32 = 1 << 2;
pub const FLAG_DEBOUNCE: u32 = 1 << 3;
pub const FLAG_OUTPUT_INIT_LOW: u32 = 0;
pub const FLAG_OUTPUT_INIT_HIGH: u32 = 1 << 4;

pub const LEVEL_LOW: u32 = 0;
pub const LEVEL_HIGH: u32 = 1;

// ── IRQ mode enum (mirrors lisa_gpio_irq_mode_t) ─────────────────────
pub const IRQ_EDGE_RISING: u32 = 0x01;
pub const IRQ_EDGE_FALLING: u32 = 0x02;
pub const IRQ_EDGE_BOTH: u32 = 0x03;
pub const IRQ_LEVEL_HIGH: u32 = 0x04;
pub const IRQ_LEVEL_LOW: u32 = 0x08;

pub type IrqCallback = unsafe extern "C" fn(pin: u32, user_data: *mut c_void);

// ── api vtable (mirrors lisa_gpio_api_t) ─────────────────────────────
#[repr(C)]
pub struct GpioApi {
    pub configure:
        Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32, flags: u32) -> c_int>,
    pub get_config:
        Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32, flags: *mut u32) -> c_int>,
    pub read_pin: Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32) -> c_int>,
    pub write_pin:
        Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32, value: u32) -> c_int>,
    pub configure_irq: Option<
        unsafe extern "C" fn(
            dev: *mut LisaDevice,
            pin: u32,
            mode: u32,
            cb: IrqCallback,
            ud: *mut c_void,
        ) -> c_int,
    >,
    pub enable_irq: Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32) -> c_int>,
    pub disable_irq: Option<unsafe extern "C" fn(dev: *mut LisaDevice, pin: u32) -> c_int>,
}

// `lisa_device_t` (opaque to us) has a `void *api` field at a fixed offset.
// We don't bind that struct directly; instead we expose helpers that read
// `dev->api` via FFI through these inline equivalents implemented in Rust.
//
// To avoid teaching Rust about the lisa_device_t layout, we cheat: the C
// inline `lisa_gpio_configure` etc. are tiny wrappers that read `dev->api`,
// cast to `lisa_gpio_api_t*`, and dispatch. We mirror that exactly:

// Layout reminder: `lisa_device_t { const char *name; lisa_device_state_t
// state; lisa_device_stats_t stats; void *api; ... }`. We DO need to read
// dev->api. To keep this binding self-contained without binding the whole
// struct, we expose tiny C helper functions in the glue shim. See
// modules/rust/arcs/glue/arcs_rust_dev.{h,c} (added in Task C1 Step 4).

extern "C" {
    /// Returns `dev->api` cast to `*const GpioApi`, or NULL if dev or its api
    /// is NULL. Defined in glue/arcs_rust_dev.c.
    pub fn arcs_rust_dev_get_api(dev: *mut LisaDevice) -> *const c_void;
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered GPIO controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour.
#[inline]
pub unsafe fn configure(dev: *mut LisaDevice, pin: u32, flags: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const GpioApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).configure };
    match f {
        Some(fp) => unsafe { fp(dev, pin, flags) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered GPIO controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour.
#[inline]
pub unsafe fn read_pin(dev: *mut LisaDevice, pin: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const GpioApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).read_pin };
    match f {
        Some(fp) => unsafe { fp(dev, pin) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered GPIO controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour.
#[inline]
pub unsafe fn write_pin(dev: *mut LisaDevice, pin: u32, value: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const GpioApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).write_pin };
    match f {
        Some(fp) => unsafe { fp(dev, pin, value) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered GPIO controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour. `cb` will
/// be invoked from IRQ context with `user_data`; both must remain valid
/// (and `cb` must be re-entrancy-safe) for as long as the IRQ stays
/// configured.
#[inline]
pub unsafe fn configure_irq(
    dev: *mut LisaDevice,
    pin: u32,
    mode: u32,
    cb: IrqCallback,
    user_data: *mut c_void,
) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const GpioApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).configure_irq };
    match f {
        Some(fp) => unsafe { fp(dev, pin, mode, cb, user_data) },
        None => -6,
    }
}

/// # Safety
///
/// Caller must hold a valid `*mut LisaDevice` previously obtained from
/// `sys::device::lisa_device_get` for a registered GPIO controller.
/// The pointer is dereferenced internally to dispatch through the
/// vtable; NULL or stale pointers are undefined behaviour.
#[inline]
pub unsafe fn enable_irq(dev: *mut LisaDevice, pin: u32) -> c_int {
    let api = unsafe { arcs_rust_dev_get_api(dev) as *const GpioApi };
    if api.is_null() {
        return -1;
    }
    let f = unsafe { (*api).enable_irq };
    match f {
        Some(fp) => unsafe { fp(dev, pin) },
        None => -6,
    }
}
