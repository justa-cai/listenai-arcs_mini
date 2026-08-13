# Rust soft-float `f32` math (`fpu_softfloat`)

Runs `f32` math from Rust using **software floating-point** — built for plain
`riscv32imac-unknown-none-elf` (soft-float `ilp32` ABI, no FPU). Every float op
lowers to a soft-float library routine, so the staticlib links cleanly against
the soft-float SDK on stable Rust 1.83.0 (no nightly, no `-Zbuild-std`). Its
companion `fpu_hardfloat` does the same math on the **hardware FPU** (full
hard-float `ilp32f` build).

```rust
let pi = leibniz_pi(100_000);          // f32 div + add  -> __divsf3 / __addsf3
let d  = dot(&[1.0, 2.0, 3.0, 4.0],
             &[0.5, 0.25, 0.125, 2.0]); // f32 mul + add  -> __mulsf3 / __addsf3
```

## Soft-float vs hard-float on RISC-V

The arcs AP core has a single-precision FPU, but **you cannot mix float ABIs in
one link**. If the Rust objects enable the F extension (`-C target-feature=+f`),
GNU `ld` tags them *single-float* and refuses to merge them with the soft-float
newlib/libgcc the SDK is built against:

```
ld: can't link soft-float modules with single-float modules
```

So there are exactly two coherent choices, one per sample:

| sample | target / ABI | float work | toolchain |
|---|---|---|---|
| **fpu_softfloat** (this one) | `rv32imac` / `ilp32` (soft) | soft-float libcalls | stable 1.83 |
| **fpu_hardfloat** | `rv32imafc` / `ilp32f` (hard) | hardware FPU (`fadd.s`…) | nightly + `-Zbuild-std` |

This sample is the soft-float side: it runs on the default core with no FPU and
no special boot/ABI config — just `f32` arithmetic done in software.

## Build & flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/fpu_softfloat -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs --reset-strategy cross-coupled \
    -s /dev/<port> -b 1500000 --verify-all 0x0 build/rust_fpu_softfloat.bin
```

Expected serial output:

```
fpu_softfloat: software f32 (rv32imac soft-float, no FPU)
fpu_softfloat: leibniz pi ~ 3.1415858 (100k terms)
fpu_softfloat: dot product = 9.375 (expect 9.375)
fpu_softfloat:done pi=3.1415858 dot=9.375
```
