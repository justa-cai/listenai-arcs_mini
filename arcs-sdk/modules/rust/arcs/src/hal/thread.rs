//! Safe wrapper over `sys::thread`.

use crate::{sys, Error, Result};
use alloc::boxed::Box;
use core::ffi::{c_void, CStr};
use core::ptr::NonNull;

/// A detached handle to a FreeRTOS task spawned from Rust.
///
/// # Lifetime model
///
/// **Threads are detached by default; `Drop` is a no-op.** The SDK's
/// `__lisa_thread_entry` trampoline calls `vTaskDelete(NULL)` immediately
/// after the user closure returns, and `vPortCleanUpTCB` frees the
/// `lisa_thread_t` struct via FreeRTOS's TCB cleanup hook. This means the
/// `lisa_thread_t*` returned from `lisa_thread_create` becomes a dangling
/// pointer as soon as the task finishes — and we have no way from Rust to
/// know whether it has finished. Calling `lisa_thread_delete` on the
/// pointer (whether from `Drop` or explicitly) risks use-after-free.
///
/// To explicitly abort a still-running task before its closure returns,
/// call [`Thread::abort`]. After `abort()` returns, the handle is
/// invalidated (consumed by-value).
///
/// For tasks that complete on their own (short-lived test threads,
/// fire-and-forget workers), simply let the `Thread` go out of scope —
/// the task self-cleans up via the SDK's TCB cleanup path.
pub struct Thread {
    handle: NonNull<sys::thread::LisaThread>,
}

unsafe impl Send for Thread {}
unsafe impl Sync for Thread {}

impl Thread {
    /// Spawn a new task.
    ///
    /// - `name`     : task name. The C side reads via `uint8_t*`; we pass the
    ///                CStr's bytes and rely on the SDK's read-only access.
    /// - `stack_size`: in bytes
    /// - `prio`     : FreeRTOS priority (higher = more urgent)
    /// - `f`        : closure to run. Owned data captured by `f` must be
    ///                `Send + 'static`.
    pub fn spawn<F>(name: &'static CStr, stack_size: u32, prio: u32, f: F) -> Result<Self>
    where
        F: FnOnce() + Send + 'static,
    {
        // Box twice: outer `Box<dyn FnOnce()>` so the type is sized for the
        // trampoline; the trampoline takes ownership and drops it.
        let boxed: Box<dyn FnOnce() + Send + 'static> = Box::new(f);
        let raw = Box::into_raw(Box::new(boxed));

        let attr = sys::thread::ThreadAttr {
            name: name.as_ptr() as *const u8,
            stack_size,
            priority: prio,
        };

        let h = unsafe { sys::thread::lisa_thread_create(&attr, trampoline, raw as *mut c_void) };
        let handle = NonNull::new(h).ok_or_else(|| {
            // Reclaim the Box so we don't leak on spawn failure.
            unsafe {
                drop(Box::from_raw(raw));
            }
            Error::NoMemory
        })?;
        Ok(Self { handle })
    }

    /// Sleep the current task for `ms` milliseconds.
    pub fn sleep_ms(ms: u32) {
        let _ = unsafe { sys::thread::lisa_thread_mdelay(ms) };
    }

    /// Yield the current task's time slice.
    pub fn yield_now() {
        let _ = unsafe { sys::thread::lisa_thread_yield() };
    }

    /// Name of the currently-running task (None if SDK returned NULL).
    pub fn current_name() -> Option<&'static CStr> {
        let p = unsafe { sys::thread::lisa_thread_cur_thread_name() };
        if p.is_null() {
            None
        } else {
            Some(unsafe { CStr::from_ptr(p) })
        }
    }

    /// Forcibly abort the task before its closure returns.
    ///
    /// Consumes `self`. There is no Rust-level join API exposed by the SDK
    /// to guarantee liveness, so prefer to let tasks self-terminate.
    ///
    /// # Safety
    ///
    /// Caller must guarantee that the task's closure has NOT yet returned.
    /// If the closure already finished, the SDK has already freed the
    /// `lisa_thread_t*` via TCB cleanup; calling `lisa_thread_delete`
    /// on the dangling pointer is undefined behaviour.
    pub unsafe fn abort(self) {
        let handle = self.handle.as_ptr();
        // Don't run Drop (which is a no-op anyway, but explicit).
        core::mem::forget(self);
        unsafe {
            let _ = sys::thread::lisa_thread_delete(handle);
        }
    }
}

impl Drop for Thread {
    /// No-op: the SDK self-deletes the task and frees `lisa_thread_t`
    /// inside the trampoline. Calling `lisa_thread_delete` from Drop
    /// would be use-after-free in the (common) case where the task
    /// has already finished. See struct-level docs.
    fn drop(&mut self) {
        // Intentionally empty — task lifetime is owned by the SDK.
    }
}

unsafe extern "C" fn trampoline(arg: *mut c_void) {
    // Reconstruct the doubly-boxed closure and run it. Box dtor drops the
    // closure's captures.
    let f = unsafe { Box::from_raw(arg as *mut Box<dyn FnOnce() + Send + 'static>) };
    f();
    // After return, FreeRTOS task self-deletion conventions are handled by
    // the SDK port; nothing more to do.
}
