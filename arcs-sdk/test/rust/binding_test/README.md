# Rust Binding Test

Smoke-tests the Rust HAL (log, thread, sync, gpio, uart, i2c/spi, pwm/rtc/adc/
flash) plus the embedded-hal/-io/-storage trait surface on `arcs_evb`. Each
subtest is a self-contained Rust function returning `arcs::Result<()>`. The
final PASSED count is the grading line that CI matches against.

The suite covers 15 subtests: the 6 concurrency/log/alloc subtests, the GPIO and
UART open subtests, two embedded-hal subtests (`eh_gpio_traits` exercising
`OutputPin`/`StatefulOutputPin` on `GpioPin`, and `eh_delay` exercising
`DelayNs` on `arcs::Delay`), the R3 peripheral subtests `pwm_configure`,
`rtc_set_get` (set time then confirm it advances), `adc_read` (internal VBAT
channel), and `flash_scratch` (erase/write/read a 1 MiB scratch sector), and the
R5 `dual_alloc` subtest (`RawBox` in SRAM vs PSRAM, asserting each pointer lands
in the right address range).

## Build & flash

```sh
bash build.sh -C -S test/rust/binding_test -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn \
    -C arcs --reset-strategy cross-coupled \
    --verify-all 0x0 build/rust_binding_test.bin
```

## Expected serial output

```
=== Rust Binding Test STARTING ===
PASS: log_facade
PASS: alloc_box_vec
PASS: thread_spawn
PASS: mutex_counter
PASS: semaphore_signal
PASS: channel
PASS: gpio_open
PASS: uart_open
PASS: eh_gpio_traits
PASS: eh_delay
PASS: pwm_configure
PASS: rtc_set_get
PASS: adc_read
PASS: flash_scratch
=== Rust Binding Test PASSED (14/14) ===
=== Rust Binding Test PASS-GATE OK ===
```
