#![no_std]

extern crate alloc;

use embedded_graphics::{
    mono_font::{ascii::FONT_10X20, MonoTextStyle},
    pixelcolor::Rgb565,
    prelude::*,
    primitives::{Circle, PrimitiveStyle, Rectangle},
    text::Text,
};

#[arcs::main]
fn main() -> arcs::Result<()> {
    // The bus/panel was attached in C (main.c) before rust_main; here we just
    // open the ready device and draw.
    let disp = arcs::Display::open(c"display")?;
    disp.blanking_off()?;
    disp.set_brightness(80)?;
    let (w, h) = (disp.width(), disp.height());
    arcs::log::info!("display_gfx: panel {}x{}", w, h);

    // Full-screen framebuffer in PSRAM (via the R5 dual allocator) used as an
    // embedded-graphics DrawTarget.
    let mut fb = arcs::display::FrameBuffer::new(&disp)?;
    fb.clear(Rgb565::new(0, 8, 16))?; // dark blue background

    Circle::new(Point::new(30, 30), 80)
        .into_styled(PrimitiveStyle::with_fill(Rgb565::RED))
        .draw(&mut fb)?;
    Rectangle::new(Point::new(130, 40), Size::new(70, 70))
        .into_styled(PrimitiveStyle::with_fill(Rgb565::GREEN))
        .draw(&mut fb)?;
    Text::new(
        "Rust + ARCS",
        Point::new(20, 170),
        MonoTextStyle::new(&FONT_10X20, Rgb565::WHITE),
    )
    .draw(&mut fb)?;

    fb.flush()?; // push the framebuffer to the panel
    arcs::log::info!("display_gfx:done — drew shapes + text, flushed to panel");
    Ok(())
}
