#![no_std]

extern crate alloc;

#[arcs::main]
fn main() -> arcs::Result<()> {
    // Each line goes to BOTH the UART console and the SEGGER RTT backend
    // (CONFIG_LOG_BACKEND_SEGGER_RTT=y). Read the RTT copy over J-Link with
    // `JLinkRTTLogger` / `JLinkRTTClient`.
    arcs::log::info!("rtt_log: start — Rust log over SEGGER RTT");
    let mut tick = 0u32;
    loop {
        arcs::log::info!("rtt_log: tick {}", tick);
        tick = tick.wrapping_add(1);
        arcs::Thread::sleep_ms(500);
    }
}
