//! Implementation detail of arcs::entry! macro. Public-but-hidden so the macro
//! can reference these symbols without polluting the crate's public surface.

use core::sync::atomic::{AtomicBool, Ordering};

static INITIALIZED: AtomicBool = AtomicBool::new(false);

/// Called once at the start of every entry-fn body the macro generates.
/// Idempotent: subsequent calls (e.g. multiple entry! macros in the same
/// binary, which is unusual but legal) are no-ops.
#[doc(hidden)]
pub fn entry_init() {
    if INITIALIZED.swap(true, Ordering::AcqRel) {
        return;
    }
    // Bridge `::log` to ARCS easylogger via our shim.
    let _ = crate::hal::log::init();
}

/// Panic handler body. Reachable only from the `#[panic_handler]` the macro
/// installs in each sample crate. Never returns.
#[doc(hidden)]
pub fn panic_impl(info: &core::panic::PanicInfo) -> ! {
    use core::fmt::Write;

    // Do NOT use Box / Vec / format! — those go through the global allocator.
    // panic must not re-panic or deadlock if the allocator itself failed.
    let mut buf = heapless::String::<256>::new();
    let _ = write!(buf, "{}", info);

    unsafe {
        crate::sys::log::arcs_rust_log(
            crate::sys::log::LEVEL_ERROR,
            c"rust-panic".as_ptr(),
            buf.as_ptr() as _,
            buf.len(),
        );
    }

    // With the `panic-reboot` feature, recover by triggering a full software
    // reset after logging (the arcs fault handler otherwise hangs forever).
    #[cfg(feature = "panic-reboot")]
    {
        // Let the panic message above physically drain from the UART before the
        // reset truncates it. Crude busy-wait — in panic context we avoid the
        // RTOS scheduler; black_box stops the loop being optimised away.
        let mut i = 0u32;
        while core::hint::black_box(i) < 2_000_000 {
            i = i.wrapping_add(1);
        }
        crate::hal::reset::reboot();
    }

    // Default: ebreak traps to the SDK's RISC-V fault handler, which dumps
    // state and then hangs (`while(1)`), leaving the core halted for
    // inspection. Loop to honor the `-> !` return type even if ebreak is
    // somehow ignored. On non-RISC-V hosts (e.g. test builds) we just spin —
    // panic_impl is never reached at runtime there because we don't install
    // the panic handler on host.
    #[cfg(not(feature = "panic-reboot"))]
    loop {
        #[cfg(target_arch = "riscv32")]
        unsafe {
            core::arch::asm!("ebreak", options(nomem, nostack, preserves_flags));
        }
        #[cfg(not(target_arch = "riscv32"))]
        core::hint::spin_loop();
    }
}
