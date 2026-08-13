//! Safe sync primitives backed by FreeRTOS via sys::sync.

use crate::{sys, Error, Result};
use core::cell::UnsafeCell;
use core::ffi::c_void;
use core::marker::PhantomData;
use core::ops::{Deref, DerefMut};
use core::ptr::NonNull;
use core::time::Duration;

// ── Mutex ────────────────────────────────────────────────────────────

pub struct Mutex<T: ?Sized> {
    raw: NonNull<sys::sync::LisaMutex>,
    data: UnsafeCell<T>,
}

unsafe impl<T: Send> Send for Mutex<T> {}
unsafe impl<T: Send> Sync for Mutex<T> {}

pub struct MutexGuard<'a, T: ?Sized + 'a> {
    mutex: &'a Mutex<T>,
}

impl<T> Mutex<T> {
    pub fn new(val: T) -> Result<Self> {
        let raw = unsafe { sys::sync::lisa_mutex_create() };
        let raw = NonNull::new(raw).ok_or(Error::NoMemory)?;
        Ok(Self {
            raw,
            data: UnsafeCell::new(val),
        })
    }

    pub fn lock(&self) -> Result<MutexGuard<'_, T>> {
        // -1 = wait forever (FreeRTOS portMAX_DELAY convention in lisa OSAL).
        Error::from_c(unsafe { sys::sync::lisa_mutex_lock(self.raw.as_ptr(), -1) })?;
        Ok(MutexGuard { mutex: self })
    }

    pub fn try_lock(&self) -> Result<MutexGuard<'_, T>> {
        Error::from_c(unsafe { sys::sync::lisa_mutex_lock(self.raw.as_ptr(), 0) })?;
        Ok(MutexGuard { mutex: self })
    }
}

impl<T: ?Sized> Drop for Mutex<T> {
    fn drop(&mut self) {
        let _ = unsafe { sys::sync::lisa_mutex_delete(self.raw.as_ptr()) };
    }
}

impl<T> Deref for MutexGuard<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        unsafe { &*self.mutex.data.get() }
    }
}

impl<T> DerefMut for MutexGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.mutex.data.get() }
    }
}

impl<T: ?Sized> Drop for MutexGuard<'_, T> {
    fn drop(&mut self) {
        let _ = unsafe { sys::sync::lisa_mutex_unlock(self.mutex.raw.as_ptr()) };
    }
}

// ── Semaphore ────────────────────────────────────────────────────────

pub struct Semaphore {
    raw: NonNull<sys::sync::LisaSemaphore>,
}

unsafe impl Send for Semaphore {}
unsafe impl Sync for Semaphore {}

impl Semaphore {
    pub fn new(count: u32) -> Result<Self> {
        let raw = unsafe { sys::sync::lisa_semaphore_create(count) };
        let raw = NonNull::new(raw).ok_or(Error::NoMemory)?;
        Ok(Self { raw })
    }

    pub fn acquire(&self) -> Result<()> {
        Error::from_c(unsafe { sys::sync::lisa_semaphore_take(self.raw.as_ptr(), -1) })
    }

    pub fn acquire_timeout(&self, t: Duration) -> Result<()> {
        let ms: i32 = t.as_millis().try_into().unwrap_or(i32::MAX);
        Error::from_c(unsafe { sys::sync::lisa_semaphore_take(self.raw.as_ptr(), ms) })
    }

    pub fn release(&self) -> Result<()> {
        Error::from_c(unsafe { sys::sync::lisa_semaphore_give(self.raw.as_ptr()) })
    }

    /// Drain all queued tokens without blocking (FreeRTOS `take` until empty).
    pub fn clear(&self) -> Result<()> {
        Error::from_c(unsafe { sys::sync::lisa_semaphore_clear(self.raw.as_ptr()) })
    }

    /// Raw FreeRTOS-backed handle, for FFI callbacks that must signal this
    /// semaphore from ISR context via `lisa_semaphore_give` (itself ISR-safe).
    /// Used by `hal::spi` to pass a stable completion handle as the SDK
    /// callback's `user_data`.
    pub(crate) fn raw_handle(&self) -> *mut sys::sync::LisaSemaphore {
        self.raw.as_ptr()
    }
}

impl Drop for Semaphore {
    fn drop(&mut self) {
        let _ = unsafe { sys::sync::lisa_semaphore_delete(self.raw.as_ptr()) };
    }
}

// ── Channel<T, N> ────────────────────────────────────────────────────

/// Bounded multi-producer/multi-consumer FIFO. Element type must be `Copy` —
/// the C queue copies bytes verbatim, so types with `Drop` would leak or
/// double-free on send/recv.
pub struct Channel<T: Copy, const N: usize> {
    raw: NonNull<sys::sync::LisaQueue>,
    _marker: PhantomData<T>,
}

unsafe impl<T: Copy + Send, const N: usize> Send for Channel<T, N> {}
unsafe impl<T: Copy + Send, const N: usize> Sync for Channel<T, N> {}

impl<T: Copy, const N: usize> Channel<T, N> {
    pub fn new() -> Result<Self> {
        // Static name buffer — SDK reads but does not write.
        static NAME: [u8; 5] = *b"chan\0";
        let raw = unsafe {
            sys::sync::lisa_queue_create(
                N as u32,
                NAME.as_ptr() as *mut u8,
                core::mem::size_of::<T>() as u32,
            )
        };
        let raw = NonNull::new(raw).ok_or(Error::NoMemory)?;
        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    pub fn send(&self, item: T) -> Result<()> {
        let mut item = item; // mutable copy so we have a real address
        Error::from_c(unsafe {
            sys::sync::lisa_queue_push(
                self.raw.as_ptr(),
                &mut item as *mut T as *mut c_void,
                core::mem::size_of::<T>() as u32,
                -1,
            )
        })
    }

    pub fn try_send(&self, item: T) -> Result<()> {
        let mut item = item;
        Error::from_c(unsafe {
            sys::sync::lisa_queue_push(
                self.raw.as_ptr(),
                &mut item as *mut T as *mut c_void,
                core::mem::size_of::<T>() as u32,
                0,
            )
        })
    }

    pub fn recv(&self) -> Result<T> {
        let mut out = core::mem::MaybeUninit::<T>::uninit();
        Error::from_c(unsafe {
            sys::sync::lisa_queue_pop(
                self.raw.as_ptr(),
                out.as_mut_ptr() as *mut c_void,
                core::mem::size_of::<T>() as u32,
                -1,
            )
        })?;
        Ok(unsafe { out.assume_init() })
    }

    pub fn try_recv(&self) -> Result<T> {
        let mut out = core::mem::MaybeUninit::<T>::uninit();
        Error::from_c(unsafe {
            sys::sync::lisa_queue_pop(
                self.raw.as_ptr(),
                out.as_mut_ptr() as *mut c_void,
                core::mem::size_of::<T>() as u32,
                0,
            )
        })?;
        Ok(unsafe { out.assume_init() })
    }
}

impl<T: Copy, const N: usize> Drop for Channel<T, N> {
    fn drop(&mut self) {
        let _ = unsafe { sys::sync::lisa_queue_delete(self.raw.as_ptr()) };
    }
}
