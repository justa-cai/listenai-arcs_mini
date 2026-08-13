//! FFI binding for system/os/inc/lisa_thread.h
//!
//! Hand-written; CI bindgen-drift check enforces consistency.
//! Last synced against C header at commit: HEAD

use core::ffi::{c_char, c_int, c_void};

/// Opaque struct mirroring `lisa_thread_t`. SDK code returns a `*mut LisaThread`
/// whose internals must not be inspected from Rust.
#[repr(C)]
pub struct LisaThread {
    _opaque: [u8; 0],
}

/// `lisa_thread_attr_t` — note `name` is `uint8_t *` in C (mutable). We bind
/// it as `*const u8` and cast at call sites; the SDK never writes through it
/// despite the C signature.
#[repr(C)]
pub struct ThreadAttr {
    pub name: *const u8,
    pub stack_size: u32,
    pub priority: u32,
}

pub type ThreadEntryFn = unsafe extern "C" fn(arg: *mut c_void);

extern "C" {
    pub fn lisa_thread_create(
        attr: *const ThreadAttr,
        entry: ThreadEntryFn,
        arg: *mut c_void,
    ) -> *mut LisaThread;

    pub fn lisa_thread_delete(thread: *mut LisaThread) -> c_int;
    pub fn lisa_thread_set_priority(thread: *mut LisaThread, priority: u8) -> c_int;
    pub fn lisa_thread_delay(ticks: u32) -> c_int;
    pub fn lisa_thread_mdelay(ms: u32) -> c_int;
    pub fn lisa_thread_yield() -> c_int;
    pub fn lisa_thread_cur_thread_name() -> *mut c_char;
}
