#![no_std]

extern crate alloc;

#[arcs::main]
fn main() -> arcs::Result<()> {
    let spi = arcs::Spi::open(c"spi0")?;
    spi.configure(&arcs::spi::Config::default())?;
    arcs::log::info!("spi_loopback: spi0 configured @ 1MHz mode0 8-bit MSB, hw-CS");

    // 1) TX-only path (mirrors the C lisa_spi master sample): proves the
    //    async-start -> ISR completion-callback -> semaphore plumbing.
    spi.write(&[0x9f, 0x12, 0x34])?;
    arcs::log::info!("spi_loopback: write 3 bytes ok");

    // 2) Full-duplex transfer; with a MOSI->MISO jumper, rx mirrors tx.
    let tx: [u8; 4] = [0xa5, 0x5a, 0xc3, 0x3c];
    let mut rx = [0u8; 4];
    spi.transfer(&tx, &mut rx)?;
    arcs::log::info!(
        "spi_loopback: xfer tx=[{:02x} {:02x} {:02x} {:02x}] rx=[{:02x} {:02x} {:02x} {:02x}]",
        tx[0],
        tx[1],
        tx[2],
        tx[3],
        rx[0],
        rx[1],
        rx[2],
        rx[3]
    );

    let looped = rx == tx;
    if looped {
        arcs::log::info!("spi_loopback: MOSI->MISO loopback detected (rx == tx)");
    } else {
        arcs::log::info!("spi_loopback: no loopback jumper (MISO idle) — transfer path OK");
    }

    arcs::log::info!("spi_loopback:done ok={}", looped as u8);
    Ok(())
}
