#![no_std]

extern crate alloc;

use core::hint::black_box;

/// Leibniz series for π — exercises f32 divide + add (`fdiv.s` / `fadd.s`).
fn leibniz_pi(terms: u32) -> f32 {
    let mut sum = 0.0f32;
    let mut sign = 1.0f32;
    let mut k = 0u32;
    while k < terms {
        let denom = (2 * k + 1) as f32;
        sum += sign / denom;
        sign = -sign;
        k += 1;
    }
    sum * 4.0
}

/// Dot product — exercises f32 multiply-accumulate (`fmul.s` / `fadd.s`).
fn dot(a: &[f32], b: &[f32]) -> f32 {
    let mut acc = 0.0f32;
    for (x, y) in a.iter().zip(b.iter()) {
        acc += x * y;
    }
    acc
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    // Full hard-float ABI: CONFIG_FPU=y builds rv32imafc/ilp32f, so the SDK
    // startup enables mstatus.FS and CONFIG_RISCV_FPU makes FreeRTOS save the F
    // registers per task. No manual FPU enable is needed here — unlike the
    // soft-float (+f) sample.
    //
    // Diagnostics: misa.F (bit 5) = does the core have the F extension;
    // mstatus.FS (bits 14:13) = is the FPU enabled for this task.
    let misa: u32;
    let mstatus: u32;
    unsafe {
        core::arch::asm!("csrr {0}, misa", out(reg) misa, options(nomem, nostack));
        core::arch::asm!("csrr {0}, mstatus", out(reg) mstatus, options(nomem, nostack));
    }
    let has_f = (misa >> 5) & 1;
    let fs = (mstatus >> 13) & 0x3;
    arcs::log::info!(
        "fpu_hardfloat: misa={:#010x} (F={}) mstatus={:#010x} (FS={})",
        misa,
        has_f,
        mstatus,
        fs
    );

    // `black_box` keeps the optimizer from const-folding the math away, so the
    // FPU is actually exercised at run time.
    let pi = leibniz_pi(black_box(100_000));
    let a = black_box([1.0f32, 2.0, 3.0, 4.0]);
    let b = black_box([0.5f32, 0.25, 0.125, 2.0]);
    let d = dot(&a, &b); // 0.5 + 0.5 + 0.375 + 8.0 = 9.375

    arcs::log::info!("fpu_hardfloat: leibniz pi ~ {} (100k terms)", pi);
    arcs::log::info!("fpu_hardfloat: dot product = {} (expect 9.375)", d);
    arcs::log::info!("fpu_hardfloat:done pi={} dot={}", pi, d);
    Ok(())
}
