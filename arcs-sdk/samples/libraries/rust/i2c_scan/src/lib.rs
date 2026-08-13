#![no_std]

extern crate alloc;

#[arcs::main]
fn main() -> arcs::Result<()> {
    let i2c = arcs::I2c::open(c"i2c0")?;
    i2c.configure(arcs::i2c::SPEED_STANDARD)?;

    arcs::log::info!("i2c_scan: probing 0x08..=0x77 on i2c0");
    let mut found = 0u32;
    for addr in 0x08u16..=0x77 {
        if i2c.probe(addr)? {
            arcs::log::info!("i2c_scan: found device at 0x{:02x}", addr);
            found += 1;
        }
    }
    arcs::log::info!("i2c_scan:done found={}", found);
    Ok(())
}
