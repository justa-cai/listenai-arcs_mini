//! Global allocator bridging to FreeRTOS `pvPortMalloc` / `vPortFree`.
//!
//! Installed by the `arcs::entry!` macro as `#[global_allocator]`.
//!
//! FreeRTOS guarantees 8-byte alignment from `pvPortMalloc`. For larger
//! alignment we over-allocate and stash the original pointer just before the
//! aligned address.

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;

extern "C" {
    fn pvPortMalloc(size: usize) -> *mut c_void;
    fn vPortFree(ptr: *mut c_void);
}

pub struct ArcsAllocator;

unsafe impl GlobalAlloc for ArcsAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.align() <= 8 {
            unsafe { pvPortMalloc(layout.size()) as *mut u8 }
        } else {
            let total = layout.size().saturating_add(layout.align());
            let raw = unsafe { pvPortMalloc(total) } as usize;
            if raw == 0 {
                return core::ptr::null_mut();
            }
            let aligned = (raw + layout.align()) & !(layout.align() - 1);
            unsafe {
                *((aligned - core::mem::size_of::<usize>()) as *mut usize) = raw;
            }
            aligned as *mut u8
        }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        if layout.align() <= 8 {
            unsafe { vPortFree(ptr as _) };
        } else {
            let raw = unsafe { *((ptr as usize - core::mem::size_of::<usize>()) as *mut usize) };
            unsafe { vPortFree(raw as _) };
        }
    }
}

// ── Dual (zone-aware) allocator ──────────────────────────────────────
//
// The default `#[global_allocator]` above is fine for general use, but some
// consumers (e.g. display framebuffers) want to choose the physical pool:
// large external PSRAM vs small/fast internal SRAM. These APIs allocate from a
// specific [`Zone`] using a stable raw-pointer interface — no nightly
// `allocator_api`.

use crate::{sys, Error, Result};
use core::ops::{Deref, DerefMut};
use core::ptr::NonNull;

/// A physical memory pool.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Zone {
    /// External PSRAM — large (multiple MiB), slower. Backed by `lisa_mem_*`.
    Psram,
    /// Internal SRAM — small (hundreds of KiB), fast. Backed by `inram_*`.
    Sram,
}

/// Allocate `layout.size()` bytes (>= 4-byte aligned) from `zone`.
/// Returns null on failure.
///
/// # Safety
/// Standard allocator contract; the returned block must be freed with
/// [`dealloc_in`] using the **same** `zone`.
pub unsafe fn alloc_in(zone: Zone, layout: Layout) -> *mut u8 {
    let align = layout.align().max(4);
    match zone {
        Zone::Psram => unsafe {
            sys::mem::lisa_mem_align_alloc(align as u32, layout.size() as u32) as *mut u8
        },
        Zone::Sram => unsafe { sys::mem::inram_malloc(align, layout.size()) as *mut u8 },
    }
}

/// Free a block obtained from [`alloc_in`] with the same `zone`.
///
/// # Safety
/// `ptr` must come from `alloc_in(zone, _)` and not have been freed.
pub unsafe fn dealloc_in(zone: Zone, ptr: *mut u8, _layout: Layout) {
    match zone {
        Zone::Psram => unsafe { sys::mem::lisa_mem_free(ptr as *mut c_void) },
        Zone::Sram => unsafe { sys::mem::inram_free(ptr as *mut c_void) },
    }
}

/// Best-effort classification of a pointer's pool by its address range on arcs
/// (observed on `arcs_evb`): internal SRAM in `0x2000_0000..0x2800_0000`
/// (`inram_malloc` returns e.g. `0x2001_xxxx`); PSRAM in
/// `0x2800_0000..0x4000_0000` (`lisa_mem_*` returns the cached alias at
/// `0x2880_xxxx`; `0x3000_0000` is the XIP/uncached mapping). Returns `None`
/// otherwise.
pub fn zone_of(ptr: *const u8) -> Option<Zone> {
    let a = ptr as usize;
    if (0x2000_0000..0x2800_0000).contains(&a) {
        Some(Zone::Sram)
    } else if (0x2800_0000..0x4000_0000).contains(&a) {
        Some(Zone::Psram)
    } else {
        None
    }
}

/// An owned, heap-allocated `T` placed in a specific [`Zone`] — like `Box<T>`
/// but pool-aware and built on the stable raw allocator (no `allocator_api`).
pub struct RawBox<T> {
    ptr: NonNull<T>,
    zone: Zone,
}

unsafe impl<T: Send> Send for RawBox<T> {}
unsafe impl<T: Sync> Sync for RawBox<T> {}

impl<T> RawBox<T> {
    /// Allocate `value` in `zone`. Returns `Err(NoMemory)` if the pool is full.
    pub fn new_in(zone: Zone, value: T) -> Result<Self> {
        let layout = Layout::new::<T>();
        let p = unsafe { alloc_in(zone, layout) } as *mut T;
        let ptr = NonNull::new(p).ok_or(Error::NoMemory)?;
        unsafe { ptr.as_ptr().write(value) };
        Ok(Self { ptr, zone })
    }

    /// The pool this allocation lives in.
    pub fn zone(&self) -> Zone {
        self.zone
    }
}

impl<T> Deref for RawBox<T> {
    type Target = T;
    fn deref(&self) -> &T {
        unsafe { self.ptr.as_ref() }
    }
}

impl<T> DerefMut for RawBox<T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { self.ptr.as_mut() }
    }
}

impl<T> Drop for RawBox<T> {
    fn drop(&mut self) {
        unsafe {
            core::ptr::drop_in_place(self.ptr.as_ptr());
            dealloc_in(self.zone, self.ptr.as_ptr() as *mut u8, Layout::new::<T>());
        }
    }
}
