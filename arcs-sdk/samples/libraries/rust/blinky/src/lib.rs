#![no_std]

extern crate alloc;

use arcs::{Gpio, Level, OutputOpts, Thread};

/// On-board LED on arcs_evb is wired to PAD_B[9]. Update if porting to a
/// board with a different LED pin (see boards/<board>/README.md).
const LED_GPIO: &core::ffi::CStr = c"gpiob";
const LED_PIN: u32 = 9;

arcs::entry!(rust_blinky_main, {
    let gpio = Gpio::open(LED_GPIO)?;
    gpio.configure_output(
        LED_PIN,
        OutputOpts {
            init_high: false,
            ..Default::default()
        },
    )?;

    let mut on = false;
    let mut tick: u32 = 0;
    loop {
        on = !on;
        gpio.write(LED_PIN, if on { Level::High } else { Level::Low })?;
        if tick % 4 == 0 {
            arcs::log::info!("blinky:tick:{}", tick);
        }
        tick = tick.wrapping_add(1);
        Thread::sleep_ms(500);
    }
});
