#![no_std]

extern crate alloc;

#[arcs::main]
fn main() -> arcs::Result<()> {
    arcs::log::info!("panic_reboot: about to panic — feature panic-reboot will log then reset");
    // Brief pause so each log→panic→reset cycle is observable on the console.
    arcs::Thread::sleep_ms(500);
    // With the `panic-reboot` feature (enabled in this sample's Cargo.toml), the
    // arcs panic handler logs the message then calls a full software reset, so
    // the device recovers (reboots) instead of hanging on the SDK fault handler.
    // `panic!` has type `!`, which coerces to the `Result<()>` return type.
    panic!("R4 panic-reboot demo: log then reboot (not hang)");
}
