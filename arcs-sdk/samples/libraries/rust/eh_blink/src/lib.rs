#![no_std]

extern crate alloc;

use embedded_hal::delay::DelayNs;
use embedded_hal::digital::OutputPin;

/// Generic over any embedded-hal OutputPin + DelayNs — proves trait
/// composition works against ARCS hardware via arcs::GpioPin / arcs::Delay.
fn blink_n<P, D>(pin: &mut P, delay: &mut D, n: u32)
where
    P: OutputPin,
    D: DelayNs,
{
    for _ in 0..n {
        let _ = pin.set_high();
        delay.delay_ms(200);
        let _ = pin.set_low();
        delay.delay_ms(200);
    }
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    let gpio = arcs::Gpio::open(c"gpiob")?;
    let mut led = gpio.pin(9).into_output(arcs::OutputOpts {
        init_high: false,
        ..Default::default()
    })?;
    let mut delay = arcs::Delay::new();

    arcs::log::info!("eh_blink: blinking 5x via generic OutputPin driver");
    blink_n(&mut led, &mut delay, 5);
    arcs::log::info!("eh_blink:done");
    Ok(())
}
