//! FFI binding for the ARCS dynamic-memory allocators (two physical pools).
//!
//! - **PSRAM** (external, large): `lisa_mem_*` from `system/os/inc/lisa_mem.h`.
//! - **SRAM** (internal, small/fast): `inram_*` from `system/heap/sysheap.h`.
//!
//! NOTE: `lisa_mem_sram_alloc` is a misleading **alias to the PSRAM allocator**
//! (it does not return internal SRAM), so true SRAM allocation goes through
//! `inram_malloc`/`inram_free`.
//!
//! Hand-written; trivial `void*`/size_t signatures, not routed through
//! bindgen-drift.

use core::ffi::c_void;

extern "C" {
    /// Aligned allocation from external PSRAM (`MALLOC_CAP_SPIRAM`).
    pub fn lisa_mem_align_alloc(align: u32, size: u32) -> *mut c_void;
    /// Free a PSRAM allocation.
    pub fn lisa_mem_free(ptr: *mut c_void);

    /// Aligned allocation from internal SRAM (`MALLOC_CAP_INTERNAL`).
    pub fn inram_malloc(align: usize, size: usize) -> *mut c_void;
    /// Free an SRAM allocation.
    pub fn inram_free(ptr: *mut c_void);
}
