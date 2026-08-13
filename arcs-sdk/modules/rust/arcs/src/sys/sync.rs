//! FFI binding for lisa_mutex.h + lisa_semaphore.h + lisa_queue.h
//!
//! Hand-written; CI bindgen-drift check enforces consistency.
//! Last synced against C header at commit: HEAD

use core::ffi::{c_int, c_void};

// ── Mutex ────────────────────────────────────────────────────────────
#[repr(C)]
pub struct LisaMutex {
    _opaque: [u8; 0],
}

extern "C" {
    pub fn lisa_mutex_create() -> *mut LisaMutex;
    pub fn lisa_mutex_lock(mutex: *mut LisaMutex, block_time: i32) -> c_int;
    pub fn lisa_mutex_unlock(mutex: *mut LisaMutex) -> c_int;
    pub fn lisa_mutex_delete(mutex: *mut LisaMutex) -> c_int;
}

// ── Semaphore ────────────────────────────────────────────────────────
#[repr(C)]
pub struct LisaSemaphore {
    _opaque: [u8; 0],
}

extern "C" {
    pub fn lisa_semaphore_create(count: u32) -> *mut LisaSemaphore;
    pub fn lisa_semaphore_take(sem: *mut LisaSemaphore, block_time: i32) -> c_int;
    pub fn lisa_semaphore_give(sem: *mut LisaSemaphore) -> c_int;
    pub fn lisa_semaphore_delete(sem: *mut LisaSemaphore) -> c_int;
    pub fn lisa_semaphore_clear(sem: *mut LisaSemaphore) -> c_int;
}

// ── Queue ────────────────────────────────────────────────────────────
#[repr(C)]
pub struct LisaQueue {
    _opaque: [u8; 0],
}

extern "C" {
    /// `queue_name` is declared `uint8_t *` (mutable!) in C but is only ever
    /// read; pass a cast `*mut u8` from a static C string.
    pub fn lisa_queue_create(count: u32, queue_name: *mut u8, item_size: u32) -> *mut LisaQueue;
    pub fn lisa_queue_push(
        q: *mut LisaQueue,
        item: *mut c_void,
        item_size: u32,
        wait: i32,
    ) -> c_int;
    pub fn lisa_queue_pop(q: *mut LisaQueue, item: *mut c_void, item_size: u32, wait: i32)
        -> c_int;
    pub fn lisa_queue_delete(q: *mut LisaQueue) -> c_int;
    pub fn lisa_queue_size(q: *mut LisaQueue) -> u32;
}
