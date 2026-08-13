#![no_std]

extern crate alloc;

use alloc::vec;

#[arcs::main]
fn main() -> arcs::Result<()> {
    // The WiFi core is already up (main.c waited on the init-done callback);
    // here we drive the wifi_manager station API.
    let wifi = arcs::Wifi::init()?;
    arcs::log::info!("wifi_scan: wifi_manager init ok");

    wifi.sta_enable()?;
    arcs::log::info!("wifi_scan: STA enabled");

    // Let the mode switch settle before the first scan.
    arcs::Thread::sleep_ms(300);

    // Scan buffer on the heap (32 × 64 B) to keep the task stack small.
    let mut aps = vec![arcs::ScanInfo::default(); 32];
    let n = wifi.scan(&mut aps)?;
    arcs::log::info!("wifi_scan: found {} AP(s)", n);

    for (i, ap) in aps[..n].iter().enumerate() {
        arcs::log::info!(
            "wifi_scan: [{}] ssid=\"{}\" rssi={} ch={} enc={} bssid={}",
            i,
            ap.ssid(),
            ap.rssi,
            ap.channel,
            arcs::wifi::encryption_name(ap.encryption_mode),
            ap.bssid(),
        );
    }

    arcs::log::info!("wifi_scan:done n={}", n);
    Ok(())
}
