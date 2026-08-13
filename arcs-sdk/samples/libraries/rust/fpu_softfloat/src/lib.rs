#![no_std]

extern crate alloc;

use core::hint::black_box;

// Software floating-point demo.
//
// This sample is built for plain `rv32imac` (no `+f`, soft-float ilp32 ABI), so
// every f32 operation below lowers to a soft-float library routine
// (`__addsf3` / `__mulsf3` / `__divsf3` in libgcc) — there are no FPU
// instructions, and the staticlib links cleanly against the soft-float SDK.
// The companion `fpu_hardfloat` sample runs the same math on the hardware FPU
// (full ilp32f hard-float build) instead.

/// Leibniz series for π — exercises f32 divide + add (soft-float libcalls).
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

/// Dot product — exercises f32 multiply-accumulate (soft-float libcalls).
fn dot(a: &[f32], b: &[f32]) -> f32 {
    let mut acc = 0.0f32;
    for (x, y) in a.iter().zip(b.iter()) {
        acc += x * y;
    }
    acc
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    arcs::log::info!("fpu_softfloat: software f32 (rv32imac soft-float, no FPU)");

    // `black_box` stops the optimizer from const-folding the math away, so the
    // soft-float routines are actually exercised at run time.
    let pi = leibniz_pi(black_box(100_000));
    let a = black_box([1.0f32, 2.0, 3.0, 4.0]);
    let b = black_box([0.5f32, 0.25, 0.125, 2.0]);
    let d = dot(&a, &b); // 0.5 + 0.5 + 0.375 + 8.0 = 9.375

    arcs::log::info!("fpu_softfloat: leibniz pi ~ {} (100k terms)", pi);
    arcs::log::info!("fpu_softfloat: dot product = {} (expect 9.375)", d);
    arcs::log::info!("fpu_softfloat:done pi={} dot={}", pi, d);
    Ok(())
}
