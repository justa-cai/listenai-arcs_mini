# ARCS SDK Rust Adapter (no_std)

Rust language binding for the ARCS SDK, mirroring the structure of
`modules/zig/`. Target audience: Rust developers writing firmware for
`arcs_evb`.

## Status

**MVP** — covers `log`, `thread`, `sync`, `gpio`, `uart`. See
`docs/superpowers/specs/2026-05-28-rust-no-std-support-design.md` for the
full design and the non-goals list.

**R1** layers `embedded-hal` / `embedded-io` trait impls on top of the
inherent HAL and adds the `#[arcs::main]` entry attribute. See
[embedded-hal / embedded-io interop](#embedded-hal--embedded-io-interop)
below.

**R2** adds the I2C and SPI buses — `arcs::I2c` (`embedded_hal::i2c::I2c`)
and `arcs::Spi` (`embedded_hal::spi::SpiBus`) — with the `i2c_scan` and
`spi_loopback` samples, both on-board verified on `arcs_evb`.

**R3** adds PWM, RTC, ADC and Flash — `arcs::Pwm`
(`embedded_hal::pwm::SetDutyCycle`), `arcs::Rtc`, `arcs::Adc`, and `arcs::Flash`
(`embedded_storage::nor_flash::NorFlash`). Verified on-board via `binding_test`
(14/14) plus the `adc_log` sample.

**R4** (debug/recovery infra) adds `arcs::reboot()` (software reset) and an
opt-in `panic-reboot` feature: a Rust panic logs its message then resets to
recover, instead of hanging on the SDK fault handler. See
[System reset & panic behaviour](#system-reset--panic-behaviour). The
defmt-framework and cross-reset panic-persistence parts of R4 are **deferred** —
see that section for why (debug probe + retained-RAM/reset-policy prerequisites
that this `arcs_evb` setup doesn't currently meet).

**R5** adds a PSRAM/SRAM **dual allocator** (`arcs::{RawBox, Zone}`,
`heap::alloc_in`/`dealloc_in`) and **display** support: `arcs::Display` plus a
PSRAM-backed `arcs::display::FrameBuffer` implementing `embedded-graphics`'
`DrawTarget` (RGB565). Verified on `arcs_evb`: `binding_test` `dual_alloc`
(15/15, SRAM `0x2001_xxxx` vs PSRAM `0x2880_xxxx`) and the `display_gfx` sample
(ST7789P3 panel init + draw + flush).

**R6** adds **audio playback**: `arcs::Audio` over the `lisa_audio` DAC
(`Audio::open(c"audio0")`, `configure_play` / `play_start` / `write` /
`flush` / `play_stop`). `write` takes `&[i16]` PCM and blocks while the
driver's DMA pool is full. Verified on `arcs_evb` via the `audio_play` sample
(integer-LUT 1 kHz + 500 Hz tones, PCM buffer in PSRAM via the R5 allocator;
the DAC clocks the PCM out in real time — serial timestamps match the tone
durations — ending in `audio_play:done`).

**R8a** adds **WiFi station** support: `arcs::Wifi` over the SDK's
`wifi_manager` (`Wifi::init` / `sta_enable` / `scan` / `connect` / `status`).
`wifi_manager` is a flat C API (not a `lisa_device`); the async WiFi core
bring-up (mac_manager + `lisa_wifi_init`) stays as C glue in the sample's
`main.c`, and Rust drives the manager. Verified on `arcs_evb` (LS26xx SoC has
on-die WiFi) via the `wifi_scan` sample — a live scan returned 24 real nearby
APs with SSID/RSSI/channel/encryption/BSSID, ending in `wifi_scan:done`.

**R8b** adds **BLE advertising**: `arcs::Bluetooth` over the `lisa_bluetooth`
stack (`Bluetooth::init` waits for the enable-complete callback, then
`start_advertising`). The advertising payload is supplied from Rust by
overriding the SDK's weak `lisa_bt_get_adv_data` / `lisa_bt_get_scan_rsp_data`
symbols. Verified on `arcs_evb` via the `ble_adv` sample — the stack
RF-calibrates, enables, and starts advertising as `Rust-ARCS`
(`ble_adv:advertising`). The BLE stack runs on the CP core
(`CONFIG_ARCS_CP_CORE=y`) — the first Rust sample to do so.

**R9** adds **async / [embassy](https://embassy.dev)** support via a new
`arcs-embassy` crate: an `embassy_executor::raw::Executor` that cooperates with
FreeRTOS (runs on a FreeRTOS task, blocks on a task notification when idle —
not `wfi` — so other RTOS tasks keep running), an `embassy-time` `Driver`
backed by the SoC's 64-bit 1 MHz monotonic timer, and a FreeRTOS-mask
`critical-section` impl. All on **stable** Rust 1.83.0 (pinned to the last
edition-2021 embassy: executor 0.9.1 / time 0.4.0, generic timer queue). Verified
on `arcs_evb` via the `async_tasks` sample — two concurrent `#[embassy_executor::task]`s
with independent `embassy_time::Timer`s ticked at 250 ms / 600 ms and interleaved
correctly (`async_tasks:ok`).

**R7** adds **hardware-FPU** `f32` math on the arcs **AP core** (hartid 0, which
has the FPU — `misa.F=1`; the default CP core does not). Two samples run on the
AP core (`CONFIG_ARCS_AP_CORE=y` + boot routing) and compute the same f32 math
with `fadd.s`/`fmul.s`/`fdiv.s`, verified on `arcs_evb` (`misa=0x40909127`,
`pi ~ 3.1415858`, `dot = 9.375`):

- **`fpu_softfloat`** — **stable Rust 1.83.0, no nightly**: stock `riscv32imac`
  target + `-C target-feature=+f` (FPU instructions, soft-float `ilp32` ABI,
  links with the soft-float SDK); enables `mstatus.FS` itself. Only the Rust
  code uses FP.
- **`fpu_hardfloat`** — **full hard-float `ilp32f` ABI** (`CONFIG_FPU=y` +
  `CONFIG_RISCV_FPU=y`, Rust `riscv32imafc` via nightly `-Zbuild-std`): the whole
  image is hard-float and FreeRTOS saves the F registers per task, so C and Rust
  both use FP. Needs a nightly toolchain (quarantined to that sample).

## Prerequisites

- [rustup](https://rustup.rs) (or `brew install rustup` on macOS)
- ARCS SDK build environment (`riscv64-unknown-elf-gcc`, CMake 3.20+, etc.)

The first build automatically installs Rust **1.83.0** and target
**riscv32imac-unknown-none-elf** as locked in `rust-toolchain.toml`. No
manual `rustup target add ...` needed.

### macOS note (Homebrew Intel)

`rustup` is keg-only on Homebrew Intel macOS, so `cargo` is not on the
default `PATH`. Prefix your environment before invoking `build.sh`:

```sh
export PATH="/usr/local/opt/rustup/bin:$HOME/.cargo/bin:$PATH"
```

(On Apple Silicon the keg lives at `/opt/homebrew/opt/rustup/bin`.)

`ARCS_BASE` is also **not** auto-exported by `source env.sh` in every
shell — set it explicitly if the build complains it cannot find the SDK:

```sh
export ARCS_BASE="$PWD"
```

`modules/rust/CMakeLists.txt` already adds the keg-only paths via
`find_program(... HINTS ...)`, so once `rustup` is installed there is no
extra CMake configuration to do.

## Architecture

Layers inside crate `arcs`:

```
hal/   ← safe wrappers (RAII + Result<T, arcs::Error>)
sys/   ← thin extern "C" FFI to lisa_*.h (hand-written; CI checks drift)
glue/  ← C shims for things Rust FFI can't comfortably do
        (variadic logging, dev->api struct access)
```

The `arcs` crate is **rlib-only** (`crate-type = ["rlib"]`); it does not
produce a standalone `libarcs.a` artifact. Each sample is its own standalone
Cargo package of `crate-type = ["staticlib"]`; it is intentionally not a member
of the SDK Rust workspace, so the manifest can be copied out of the SDK tree
without preserving a fixed `../../../../modules/rust` path. Sample dependencies
declare normal versions such as `arcs = "0.1.0"`. The CMake helper injects
local SDK crate paths at build time. The sample's staticlib (`lib<sample>.a`)
bundles the arcs rlib code together with the sample's `arcs::entry!`-injected
panic handler and global allocator.

The shared CMake helper `rust_app.cmake` contains the reusable Cargo
staticlib build/link logic. `rust_sample.cmake` keeps the sample-facing
`listenai_maybe_enable_rust_target()` wrapper, builds the selected manifest via
`cargo build --manifest-path <sample>/Cargo.toml -p <package>`, and injects
SDK-local crates such as `arcs` and `arcs-embassy` through a generated Cargo
patch config file. It then links the resulting `.a` (with `--whole-archive` so
`#[no_mangle] extern "C"` entry points survive linker DCE) and injects the
`arcs-rust-glue` target's C shim sources plus compile usage requirements into
the SDK executable. This avoids directly linking the OBJECT target on SDK link
paths that would otherwise turn it into a `-l` flag. External SDK apps may
reuse `rust_app.cmake`; set the CMake variable or environment variable
`ARCS_RUST_DIR` when the helper cannot infer the SDK `modules/rust` directory
from its own location or `ARCS_BASE`.

Samples live under `samples/libraries/rust/`. Tests live under
`test/rust/`. C remains the program entry; Rust is invoked as an
`extern "C"` subroutine.

## Quick start

See `samples/libraries/rust/helloworld/`. The verified build command is:

```sh
# Build (note: -C clears the build dir; -S selects the sample source dir)
bash build.sh -C -S samples/libraries/rust/helloworld -DBOARD=arcs_evb
```

The output ELF/BIN land under `build/`. Watch for the cargo build line
in the CMake log — that confirms `listenai_maybe_enable_rust_target()`
picked up the sample.

## Flash + verify recipe

Verified on macOS with the bundled `cskburn` and a USB-CDC serial port.
Adjust the cskburn binary path and `/dev/cu.usbmodem*` device name for
your machine.

```sh
# Flash (cskburn — cross-coupled reset is required for arcs_evb on macOS)
../cskburn/build/cskburn/cskburn \
    -C arcs --reset-strategy cross-coupled \
    --verify-all 0x0 build/rust_helloworld.bin

# Capture serial output (Python pyserial)
python3 -m serial.tools.miniterm /dev/cu.usbmodem* 921600
```

Expected `helloworld` output:

```
=== Rust Language Support Test ===
Hello from Rust on ARCS SDK!
alloc test: sum=6
=== Rust Test PASSED ===
```

Expected `blinky` behaviour: on-board LED toggles at ~500 ms and the log
prints `blinky:tick:N` every fourth toggle.

## Entry point: `#[arcs::main]`

`#[arcs::main]` (re-exported from the host-compiled `arcs-macros` crate) is
the recommended way to declare a Rust entry. Write an idiomatic
`fn main() -> arcs::Result<()>` and the attribute injects the
`#[global_allocator]`, `#[panic_handler]`, and runtime/log init for you —
the same machinery as `arcs::entry!`, but with attribute syntax and a
**fixed export symbol** `rust_main`:

```rust
#![no_std]
extern crate alloc;

use embedded_hal::digital::OutputPin;

#[arcs::main]
fn main() -> arcs::Result<()> {
    let gpio = arcs::Gpio::open(c"gpiob")?;
    let mut led = gpio.pin(9).into_output(Default::default())?;
    let _ = led.set_high();
    Ok(())
}
```

The C side then declares and calls the fixed symbol:

```c
extern int rust_main(void);
/* ... call rust_main() from main() ... */
```

`arcs::entry!(<symbol>, { ... })` is **retained** — use it when you need an
explicit/custom export symbol name. Exactly **one** of `#[arcs::main]` or
`arcs::entry!` is allowed per crate: both define the crate-root
`#[global_allocator]` / `#[panic_handler]` singletons.

## embedded-hal / embedded-io interop

`embedded-hal` is a **default-on** Cargo feature of the `arcs` crate
(`default = ["embedded-hal"]`, pulling in `embedded-hal = 1.0` +
`embedded-io = 0.6`). Building `--no-default-features` compiles the trait
impls out, leaving the pure inherent HAL. With the feature on, community
driver crates written against these traits work directly against ARCS
peripherals:

- **`GpioPin`** — a single-pin handle obtained via `Gpio::pin(n)`, then
  configured builder-style with `.into_output(opts)?` / `.into_input(opts)?`
  (`.level()?` reads). It implements
  `embedded_hal::digital::{OutputPin, InputPin, StatefulOutputPin}`. (These
  traits operate on one pin via `&mut self` with no pin argument, hence the
  newtype.)
- **`Uart`** implements `embedded_io::{Read, Write}`. `Write::write`
  delegates to `write_all` (full-length return); `flush` is a no-op.
- **`Delay`** — `Delay::new()` is a zero-sized handle implementing
  `embedded_hal::delay::DelayNs` (backed by `Thread::sleep_ms`; sub-ms
  delays round up to a single 1 ms tick).
- **`I2c`** — `I2c::open(c"i2c0")?` + `configure(speed)`; blocking
  `write` / `read` / `write_read` / `probe`. Implements
  `embedded_hal::i2c::I2c` (7-bit addressing). See `i2c_scan`.
- **`Spi`** — `Spi::open(c"spi0")?` + `configure(&spi::Config)`; blocking
  `transfer` / `write` / `read`. The lisa SPI ops are asynchronous
  (completion fires from the SPI ISR); the wrapper registers an internal
  completion callback that releases a semaphore and blocks on it, so the API
  is synchronous. Implements `embedded_hal::spi::SpiBus<u8>`. See
  `spi_loopback`.
- **`Pwm`** — `Pwm::open(c"pwm0", channel)?` + `set_frequency` / `configure` /
  `set_duty_percent` / `enable` / `disable`. Implements
  `embedded_hal::pwm::SetDutyCycle` (`max_duty_cycle() = 100`).
- **`Rtc`** / **`DateTime`** — `Rtc::open(c"rtc0")?` + `set(&DateTime)` /
  `now()`. `DateTime.year` is an offset from 2000 (0..=127). No embedded-hal RTC
  trait exists in 1.0, so this is inherent-only.
- **`Adc`** — `Adc::open(c"adc0")?` + `configure_channel` / `read(ch) -> u16`
  (channels 6/7 are internal VBAT/TEMP), plus `adc::raw_to_mv`. embedded-hal 1.0
  has no blocking ADC trait, so inherent-only.
- **`Flash`** — `Flash::open(c"flash0")?` + `read` / `write` / `erase` /
  `capacity` (writes need a prior erase). Implements
  `embedded_storage::nor_flash::{ReadNorFlash, NorFlash}` (`ERASE_SIZE = 4096`,
  `WRITE_SIZE = 4`).
- **`Display`** / **`display::FrameBuffer`** — `Display::open(c"display")?`
  (bus/panel attached in C beforehand) + `width`/`height`/`blanking_off`/
  `set_brightness`/`write`. `FrameBuffer::new(&display)?` allocates a full-screen
  RGB565 framebuffer in PSRAM and implements `embedded_graphics_core::DrawTarget`
  + `OriginDimensions`; draw with `embedded-graphics`, then `flush()` to the
  panel. See `display_gfx`.
- **Dual allocator** — `arcs::Zone::{Psram, Sram}` with `heap::alloc_in` /
  `dealloc_in` and `arcs::RawBox<T>` (a pool-aware `Box`, stable — no
  `allocator_api`). PSRAM via `lisa_mem_*`, internal SRAM via `inram_*`.
- **`Audio`** / **`audio::PlayConfig`** — `Audio::open(c"audio0")?` +
  `configure_play(&PlayConfig)` (sample rate / channels / bits / gain /
  DMA-pool size) + `play_start` / `write(&[i16])` / `write_all` / `flush` /
  `play_stop`. `write` copies PCM into the driver's DMA pool and blocks while
  it is full (DMA drains it). No embedded-hal audio trait exists, so
  inherent-only. See `audio_play`.
- **`Wifi`** / **`ScanInfo`** — `Wifi::init()?` (`wifi_mgr_init`) +
  `sta_enable` / `scan(&mut [ScanInfo]) -> count` / `connect(ssid, pwd)` /
  `disconnect` / `status`. `ScanInfo` exposes `ssid()` / `bssid()` / `rssi` /
  `channel` / `encryption_mode` (+ `wifi::encryption_name`). The WiFi core
  bring-up (mac_manager + async `lisa_wifi_init`) is C glue done before
  `rust_main`; `wifi_manager` is a flat C API, not a `lisa_device`. See
  `wifi_scan`.
- **`Bluetooth`** — `Bluetooth::init()?` (brings up `lisa_bluetooth`, blocks for
  the enable-complete callback) + `start_advertising` / `stop_advertising`
  (+ `_set(adv_id, adv_type)` variants) with `bt::ADV_*` / `bt::AD_TYPE_*` /
  `bt::FLAG_*` constants for building advertising payloads. The payload bytes
  are provided by overriding the weak `lisa_bt_get_adv_data` /
  `lisa_bt_get_scan_rsp_data` C symbols from the app. Inherent-only. See
  `ble_adv`.
- **`arcs-embassy`** (separate crate) — `arcs_embassy::run(|spawner| { .. })`
  runs an embassy executor on the current FreeRTOS task (never returns);
  `embassy_time::{Timer, Duration, Instant}` work via the bundled time driver;
  `arcs_embassy::Delay` implements `embedded_hal_async::delay::DelayNs`. The app
  compiles `arcs-embassy/glue/arcs_embassy_glue.c`. See `async_tasks`.
- **`arcs::Error`** implements `embedded_hal::digital::Error` +
  `embedded_io::Error` + `embedded_hal::i2c::Error` +
  `embedded_hal::spi::Error` + `embedded_hal::pwm::Error` +
  `embedded_storage::nor_flash::NorFlashError`, so it serves as the associated
  `Error` type for the impls above.

> **Breaking change (pre-1.0):** the inherent `Uart::read` was renamed to
> `Uart::read_bytes` so it no longer clashes with the `embedded_io::Read::read`
> trait method (which now provides `read`).

See `samples/libraries/rust/eh_blink` for a generic `OutputPin` + `DelayNs`
blink driver wired up via `#[arcs::main]`.

## System reset & panic behaviour

`arcs::reboot()` performs a full software reset (`sys_platform_sw_full_reset`);
it does not return. Note this is a *full* reset (POR-equivalent) — SRAM is not
preserved across it.

By default a Rust panic logs its message + location (tag `rust-panic`, ERROR)
then executes `ebreak`, which traps into the SDK's RISC-V fault handler. That
handler dumps registers and then **hangs** (`while(1)`) — ideal for inspecting a
halted core, but the device stays stuck until an external reset.

The opt-in **`panic-reboot`** Cargo feature changes this: after logging, the
panic handler calls `arcs::reboot()` so the device **recovers** instead of
hanging. Enable it per-sample:

```toml
arcs = { version = "0.1.0", features = ["panic-reboot"] }
```

See `samples/libraries/rust/panic_reboot` (on-board verified on `arcs_evb`:
panic logs `panicked at <file:line>: <msg>` then resets — reset reason becomes
`WATCHDOG SOFTWARE` — and reboots in a loop). Pair it with synchronous logging
(`CONFIG_EASYLOGGER_LOG_MODE_ASYNC=n`) so the panic message reaches the UART
before the reset.

### Deferred: defmt + cross-reset panic persistence

The rest of the R4 roadmap is **deferred** because this `arcs_evb` setup does
not currently meet its prerequisites:

- **RTT logging** is now **wired**: the `rtt_log` sample enables
  `CONFIG_LOG_BACKEND_SEGGER_RTT=y`, and because Rust `log` bridges to easylogger
  (whose single output hook is then redirected to `SEGGER_RTT_printf`), Rust log
  lines are written to the RTT control block with no Rust-side change. This is
  **firmware-side verified** on `arcs_evb`: with the RTT backend on, UART log
  output stops after easylogger init (output redirected to RTT) while the
  firmware keeps running on the CP core.
  - **Host-side RTT read is verified** (J-Link PLUS V11, **J-Link software
    V9.46**). The full Rust log streams over RTT:
    ```
    I/rust ... rtt_log: start — Rust log over SEGGER RTT
    I/rust ... rtt_log: tick 0
    I/rust ... rtt_log: tick 1
    ...
    ```
    Setup + gotchas that got it working:
    Deploy the official ListenAI `JLinkDevices` package so `ARCS` resolves as a
    RISC-V device. On macOS that DB must live in
    `~/Library/Application Support/SEGGER/JLinkDevices` (the `~/.config/SEGGER`
    path the package/zip uses is Linux-only):
    ```sh
    curl -L -o /tmp/JLinkDevices.zip \
      http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/JLinkDevices.zip
    unzip -o /tmp/JLinkDevices.zip -d ~/.config/SEGGER          # Linux
    # macOS also: cp -R ~/.config/SEGGER/JLinkDevices/. \
    #   "~/Library/Application Support/SEGGER/JLinkDevices/"
    ```
    J-Link **software version matters**: the old V7.52a (2021) cannot complete a
    cJTAG connect to ARCS, but **V9.46 can** — verified here, it connects to the
    CP core (`JTAG ID 0x10200A6D (RISC-V)`, `RISC-V RV32 detected`,
    `Connected to target`). Samples run on **core1 (CP)** → use
    `scripts/arcs/jtagscan1.JLinkScript`.
    Two more gotchas for reading RTT:
    1. **The RTT control block is below J-Link's default search range.** The
       device DB declares WorkRAM at `0x20020000`, but `_SEGGER_RTT` lands lower
       (e.g. `0x20011650` for one `rtt_log` build — get it from the ELF:
       `riscv64-unknown-elf-nm build/rust_rtt_log | grep _SEGGER_RTT`). So pass
       the address explicitly, e.g. via the GDB monitor `exec SetRTTAddr <addr>`.
    2. **Detach gracefully.** Killing the J-Link tools with `kill -9` leaves the
       probe's cJTAG session wedged (it still enumerates via `ShowEmuList`, but
       subsequent target connects hang); recover with a **USB replug**.
    3. **SDK fix required.** The SEGGER-RTT log backend had a bug
       (`SEGGER_RTT_printf` instead of `SEGGER_RTT_Write`) that garbled every
       line past the first — fixed in `system/log/lisa_log_backend_segger_rtt.c`.
    Recipe (V9.46, CP core):
    ```sh
    JLinkGDBServerCLExe -device ARCS -if cJTAG -speed 4000 -noir \
      -JLinkScriptFile <…/scripts/arcs/jtagscan1.JLinkScript> &
    ```
    Then read the RTT up-buffer. The bundled `JLinkRTTClient`/`JLinkRTTLogger`
    didn't locate the control block here (below default search range; logger
    lacks `-JLinkScriptFile`), so the reliable method was a tiny GDB client over
    `:2331` that reads `_SEGGER_RTT` from RAM directly (parse the control block,
    then read up-buffer `[RdOff,WrOff)`). The core is halted on GDB attach, which
    gives a clean snapshot.
  - **Caveat for J-Link debugging:** the PWM pinmux routes `PA0`, which is also
    the cJTAG **SWCLK** pin — so a firmware that enables PWM (e.g. `binding_test`)
    breaks the debug link. Use a non-PWM firmware (like `rtt_log`) when attaching
    J-Link.
  - True `defmt` (deferred-formatting) integration is a larger, separate effort,
    still deferred.
- **Cross-reset panic persistence** ("report the last panic on next boot") needs
  a reset that preserves SRAM plus a retained/noinit region. On arcs the only
  reset is `sys_platform_sw_full_reset` (full reset, clears SRAM) and faults
  hang rather than reset, so there is no SRAM-preserving path to carry a panic
  record across a reboot without dedicated always-on RAM support.

## Per-sample required scaffolding

Every Rust sample needs the following files alongside `Cargo.toml` and
`src/`:

- **`CMakeLists.txt`** — calls `listenai_add_executable()` then
  `include($ENV{ARCS_BASE}/modules/rust/rust_sample.cmake)` then
  `listenai_maybe_enable_rust_target(${PROJECT_NAME})`. Set
  `LISTENAI_RUST_APP` to `${CMAKE_CURRENT_SOURCE_DIR}` (directory holding
  the sample's `Cargo.toml`).
- **`Kconfig`** — a single line stub required by the SDK:
  ```
  osource "$ARCS_BASE/Kconfig"
  ```
  Copy verbatim from `helloworld`, `blinky`, or `binding_test`.
- **`prj.conf`** — at minimum:
  ```
  CONFIG_LOG_FRONTEND_EASYLOGGER=y
  ```
  Plus any driver gates the sample needs. GPIO/UART drivers are
  Kconfig-gated, so e.g. blinky (LED on `gpiob:9`) needs:
  ```
  CONFIG_LISA_GPIO_DEVICE=y
  CONFIG_LISA_GPIOB=y
  ```
  For UART: `CONFIG_LISA_UART_DEVICE=y` plus the specific controller,
  e.g. `CONFIG_LISA_UART1=y`. For other GPIO banks: `CONFIG_LISA_GPIOA=y`,
  etc.
- **`src/main.c`** — the real C entry. Declares
  `extern int <your_rust_fn>(void);` and calls it from `main()`.
- **`src/lib.rs`** — Rust crate using `arcs::entry!(<your_rust_fn>, { ... })`
  to inject the panic handler / global allocator / log init and emit a
  `#[no_mangle] extern "C" fn <your_rust_fn>` symbol. Alternatively, annotate
  `fn main() -> arcs::Result<()>` with `#[arcs::main]`, which does the same
  injection but always exports the fixed symbol `rust_main` (see
  [Entry point: `#[arcs::main]`](#entry-point-arcsmain)).

The `Cargo.toml` skeleton:

```toml
[package]
name    = "my_sample"
version = "0.1.0"
edition = "2021"
rust-version = "1.83"
license = "Apache-2.0"

[workspace]

[lib]
crate-type = ["staticlib"]
path       = "src/lib.rs"

[dependencies]
arcs = "0.1.0"
log  = { version = "0.4", default-features = false }
```

Regular Rust samples under `samples/libraries/rust/` are standalone packages.
Adding a normal sample should not require editing `modules/rust/Cargo.toml`.
The empty `[workspace]` table makes the sample its own workspace root, avoiding
accidental capture by any parent workspace after copy-out. The checked-in
`Cargo.lock` belongs to that sample app; SDK-local crates are supplied by
`rust_sample.cmake` through Cargo `--config` path patches. `fpu_hardfloat` uses
the same dependency style but keeps its sample-local nightly/build-std
toolchain and `.cargo/config.toml`.

## Adding a sys binding for a new C header

1. Create `arcs/src/sys/<module>.rs` with hand-written `extern "C"`
   declarations. Follow the pattern in existing files (header comment
   block citing the C header path and last-synced commit, `#[repr(C)]`
   structs, opaque handles via `pub struct Foo { _opaque: [u8; 0] }`).
2. Register the new module in `arcs/src/sys/mod.rs`.
3. If the C API uses variadic functions or struct field access that
   Rust FFI cannot bind to directly, add a tiny C shim under
   `arcs/glue/arcs_rust_<thing>.{h,c}` and bind that instead. Append the
   shim source to the `arcs-rust-glue` OBJECT library in
   `modules/rust/CMakeLists.txt`.
4. Add the header path to `tools/rust/bindgen-drift.sh` so CI catches
   drift against the C declarations.
5. Build with `bash build.sh -C -S samples/libraries/rust/helloworld
   -DBOARD=arcs_evb` and fix any errors. Run `./tools/rust/bindgen-drift.sh`
   locally to confirm the new binding lines up.

## Adding a HAL wrapper

Wrap the sys binding with RAII + `Result<T, Error>`. See
`arcs/src/hal/sync.rs` for the template: `NonNull<sys::*>` handle field,
`unsafe impl Send`/`Sync` where appropriate, explicit `Drop` calling the
matching `lisa_*_delete`. Re-export the public type from
`arcs/src/lib.rs`.

### Notable lifetime / API quirks

- **`Thread::Drop` is a no-op.** The SDK self-deletes the FreeRTOS task
  via `vTaskDelete(NULL)` and frees the `lisa_thread_t` struct via
  `vPortCleanUpTCB` as the trampoline returns. Calling
  `lisa_thread_delete` from Rust would be use-after-free for a task that
  has already exited. To explicitly abort a still-running task, call
  `unsafe { thread.abort() }` (consumes `self`).
- **`Semaphore::new(N)` is the maximum count** (initial count is always
  0). For a binary semaphore used as a signal, pass `Semaphore::new(1)`.
  Passing `N = 0` triggers a hard FreeRTOS `configASSERT(uxMaxCount != 0)`
  and panics the SoC — there is no graceful error path.
- **`Channel<T, N>` requires `T: Copy`**; the SDK queue copies bytes
  verbatim and `Drop` types would leak.

## CI

See `docs/rust/ci.md` for the four GitLab jobs (`rust:lint`,
`rust:build`, `rust:bindgen-drift`, `rust:onboard-binding-test`) and the
"On failure" SOP for each.
