//! System reset.
//!
//! Note on arcs fault behaviour: the SDK's default RISC-V exception handler
//! dumps registers and then **hangs** (`while(1)`) — a fault/`ebreak` does not
//! reboot. `reboot()` is the only way to recover programmatically, and is what
//! the optional `panic-reboot` feature uses to turn a Rust panic into a clean
//! "log then reset" instead of an indefinite hang.

use crate::sys;

/// Perform a full software reset of the chip. Does not return.
///
/// This is a *full* reset (POR-equivalent): SRAM is not preserved across it, so
/// it cannot be used to carry state into the next boot.
pub fn reboot() -> ! {
    unsafe {
        sys::reset::sys_platform_sw_full_reset();
    }
    // sys_platform_sw_full_reset does not return; spin as a safety net so the
    // `-> !` contract holds even if the reset is somehow delayed.
    loop {
        #[cfg(target_arch = "riscv32")]
        unsafe {
            core::arch::asm!("wfi", options(nomem, nostack, preserves_flags));
        }
        #[cfg(not(target_arch = "riscv32"))]
        core::hint::spin_loop();
    }
}
