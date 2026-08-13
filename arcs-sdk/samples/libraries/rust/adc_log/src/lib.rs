#![no_std]

extern crate alloc;

#[arcs::main]
fn main() -> arcs::Result<()> {
    let adc = arcs::Adc::open(c"adc0")?;

    // Internal channels need no external pin / pinmux: 6 = VBAT, 7 = TEMP.
    adc.configure_channel(6, arcs::adc::REF_VDD_3V6, arcs::adc::RESOLUTION_10BIT)?;
    adc.configure_channel(7, arcs::adc::REF_VDD_1V2, arcs::adc::RESOLUTION_10BIT)?;

    let vbat = adc.read(6)?;
    let temp = adc.read(7)?;

    arcs::log::info!(
        "adc_log: VBAT(ch6) raw={} ~{}mV (pre-divider)",
        vbat,
        arcs::adc::raw_to_mv(vbat, 3600, 10)
    );
    arcs::log::info!(
        "adc_log: TEMP(ch7) raw={} ~{}mV",
        temp,
        arcs::adc::raw_to_mv(temp, 1200, 10)
    );
    arcs::log::info!("adc_log:done");
    Ok(())
}
