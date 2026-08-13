# Rust Display + embedded-graphics Demo

Draws shapes and text on the `arcs_evb` ST7789P3 panel using
[`embedded-graphics`](https://crates.io/crates/embedded-graphics) on top of
`arcs::display`.

- `src/main.c` does the board-specific bring-up: pinmux for the LCD pins and
  `lisa_display_attach_bus()` (panel `st7789p3`, SPI1 4-wire, PWM0 backlight) —
  mirroring `samples/drivers/devices/lisa_display/display_flush`. Then it calls
  `rust_main`.
- `src/lib.rs` opens the ready `"display"` device, allocates a full-screen RGB565
  framebuffer in **PSRAM** (via the R5 dual allocator, `arcs::Zone::Psram`),
  wraps it as an `embedded-graphics` `DrawTarget` (`arcs::display::FrameBuffer`),
  draws a red circle + green square + "Rust + ARCS" text, and flushes it to the
  panel.

## Build & flash
```sh
export ARCS_BASE=$(pwd)
bash build.sh -C -S samples/libraries/rust/display_gfx -DBOARD=arcs_evb
../cskburn/build/cskburn/cskburn -C arcs -s <serial> -b 1500000 \
    --reset-strategy cross-coupled --verify-all 0x0 build/rust_display_gfx.bin
```

## Expected
On the panel: a red circle, a green square, and white "Rust + ARCS" text on a
dark-blue background. Serial:
```
display_gfx: panel 240x320
display_gfx:done — drew shapes + text, flushed to panel
```

> Note: the LCD uses PA0/PA1 (shared with cJTAG SWCLK/SWDIO), so don't attach
> J-Link while running this sample.
