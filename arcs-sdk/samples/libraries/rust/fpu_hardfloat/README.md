# Rust FPU — full hard-float ABI (`fpu_hardfloat`)

Runs `f32` math from Rust on the **hardware FPU of the arcs AP core** (R7),
verified on `arcs_evb`, using the **full hard-float `ilp32f` ABI**: the whole
image (C SDK + Rust) is built `rv32imafc / ilp32f`, and FreeRTOS saves the F
registers per task, so **any code — C or Rust, across multiple tasks — can use
FP**. Its companion `fpu_softfloat` does the same math with no nightly.

```rust
let pi = leibniz_pi(100_000);          // fdiv.s / fadd.s
let d  = dot(&[1.0, 2.0, 3.0, 4.0],
             &[0.5, 0.25, 0.125, 2.0]); // fmul.s / fadd.s
```

## Configuration

```
CONFIG_ARCS_AP_CORE=y       # hartid 0 — the core with the FPU (misa.F=1)
CONFIG_BOOT=y               # single self-contained image
CONFIG_BOOT_APP_CORE_AP=y   # boot calls the app on the AP core
CONFIG_FPU=y                # C SDK -> rv32imafc / ilp32f, startup enables FS
CONFIG_RISCV_FPU=y          # FreeRTOS saves/restores F registers per task
```

The Rust side must match the `ilp32f` ABI. `riscv32imafc-unknown-none-elf` ships
a precompiled `compiler_builtins` tagged **soft-float**, which GNU `ld` refuses
to merge with the single-float SDK. The fix is `-Zbuild-std` (see
`.cargo/config.toml`) to rebuild `core`/`alloc`/`compiler_builtins` from source
with the `ilp32f` attribute — which needs **nightly** (pinned in this sample's
`rust-toolchain.toml`, quarantined here so the rest of the tree stays on stable
1.83.0). `rust_sample.cmake` selects the `riscv32imafc` target automatically
when `CONFIG_FPU` is set.

Because the SDK is hard-float, the startup enables `mstatus.FS` and the FreeRTOS
port (`CONFIG_RISCV_FPU`) manages per-task FP context — so unlike `fpu_softfloat`
this sample needs **no** manual `mstatus.FS` write.

> Note: `CONFIG_RISCV_FPU`'s context-switch F-register save is unconditional, so
> it must only be used on the FPU-bearing AP core — on the CP core (no FPU) the
> `FPSTORE` is illegal and traps. That's a core-selection constraint, not a port
> bug.

## Build, verify, flash (arcs_evb)

```sh
export ARCS_BASE=$PWD
bash build.sh -C -S samples/libraries/rust/fpu_hardfloat -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs --reset-strategy cross-coupled \
    -s /dev/<port> -b 1500000 --verify-all 0x0 build/rust_fpu_hardfloat.bin
```

Verified serial output (note `mstatus FS=3` = Dirty, vs the soft-float
sample's FS=1 — the hard-float startup/RTOS path manages it):

```
Running on cpu-id: 0
fpu_hardfloat: misa=0x40909127 (F=1) mstatus=0x80006088 (FS=3)
fpu_hardfloat: leibniz pi ~ 3.1415858 (100k terms)
fpu_hardfloat: dot product = 9.375 (expect 9.375)
fpu_hardfloat:done pi=3.1415858 dot=9.375
```
