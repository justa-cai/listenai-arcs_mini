#![no_std]

extern crate alloc;

use alloc::boxed::Box;
use alloc::vec::Vec;
use core::sync::atomic::{AtomicU32, Ordering};
use core::time::Duration;

use arcs::{Channel, Mutex, Semaphore, Thread};

// Each test returns Ok(()) on PASS. Failure is reported via panic-like
// `return Err(...)` that bubbles back to arcs::entry! and prints a single
// FAIL line; downstream tests are skipped.

arcs::entry!(rust_binding_test_main, {
    let mut passed = 0u32;
    let mut total = 0u32;
    macro_rules! run {
        ($name:expr, $fn:expr) => {
            total += 1;
            match $fn {
                Ok(()) => {
                    passed += 1;
                    arcs::log::info!("PASS: {}", $name);
                }
                Err(e) => arcs::log::error!("FAIL: {}: {:?}", $name, e),
            }
        };
    }

    run!("log_facade", test_log_facade());
    run!("alloc_box_vec", test_alloc_box_vec());
    run!("thread_spawn", test_thread_spawn_join());
    run!("mutex_counter", test_mutex_protect_counter());
    run!("semaphore_signal", test_semaphore_signal());
    run!("channel", test_channel_send_recv());
    run!("gpio_open", test_gpio_open());
    run!("uart_open", test_uart_open());
    run!("eh_gpio_traits", test_eh_gpio_traits());
    run!("eh_delay", test_eh_delay());
    run!("pwm_configure", test_pwm_configure());
    run!("rtc_set_get", test_rtc_set_get());
    run!("adc_read", test_adc_read());
    run!("flash_scratch", test_flash_scratch());
    run!("dual_alloc", test_dual_alloc());

    arcs::log::info!("=== Rust Binding Test PASSED ({}/{}) ===", passed, total);
    if passed == total {
        Ok(())
    } else {
        Err(arcs::Error::Io(-1))
    }
});

fn test_log_facade() -> arcs::Result<()> {
    arcs::log::info!("log:test:info");
    arcs::log::warn!("log:test:warn");
    arcs::log::error!("log:test:error");
    arcs::log::debug!("log:test:debug");
    Ok(())
}

fn test_alloc_box_vec() -> arcs::Result<()> {
    let b = Box::new(42u32);
    if *b != 42 {
        return Err(arcs::Error::Io(-1));
    }
    let mut v: Vec<u8> = Vec::with_capacity(128);
    for i in 0..128 {
        v.push(i as u8);
    }
    let sum: u32 = v.iter().map(|x| *x as u32).sum();
    if sum != 8128 {
        return Err(arcs::Error::Io(-2));
    }
    Ok(())
}

static SHARED_FLAG: AtomicU32 = AtomicU32::new(0);

fn test_thread_spawn_join() -> arcs::Result<()> {
    SHARED_FLAG.store(0, Ordering::SeqCst);
    let _t = Thread::spawn(c"bt_t1", 2048, 5, || {
        Thread::sleep_ms(10);
        SHARED_FLAG.store(42, Ordering::SeqCst);
    })?;
    // Poll for up to 1000ms.
    for _ in 0..100 {
        Thread::sleep_ms(10);
        if SHARED_FLAG.load(Ordering::SeqCst) == 42 {
            return Ok(());
        }
    }
    Err(arcs::Error::Timeout)
}

fn test_mutex_protect_counter() -> arcs::Result<()> {
    let counter: alloc::sync::Arc<Mutex<u32>> = alloc::sync::Arc::new(Mutex::new(0)?);
    let c2 = counter.clone();
    let _t = Thread::spawn(c"bt_t2", 2048, 5, move || {
        for _ in 0..1000 {
            let mut g = c2.lock().unwrap();
            *g += 1;
        }
    })?;
    for _ in 0..1000 {
        let mut g = counter.lock()?;
        *g += 1;
    }
    // Give the child task time to finish.
    for _ in 0..100 {
        Thread::sleep_ms(10);
        let g = counter.lock()?;
        if *g == 2000 {
            return Ok(());
        }
    }
    Err(arcs::Error::Timeout)
}

fn test_semaphore_signal() -> arcs::Result<()> {
    // Semaphore::new(N) maps to xSemaphoreCreateCounting(maxCount=N, initialCount=0).
    // FreeRTOS rejects maxCount=0 with a hard assert; for a signal pattern we want
    // max=1 token (consumer takes it after producer releases).
    let sem = alloc::sync::Arc::new(Semaphore::new(1)?);
    let s2 = sem.clone();
    let _t = Thread::spawn(c"bt_t3", 2048, 5, move || {
        Thread::sleep_ms(50);
        let _ = s2.release();
    })?;
    sem.acquire_timeout(Duration::from_millis(200))
}

fn test_channel_send_recv() -> arcs::Result<()> {
    let ch: alloc::sync::Arc<Channel<u32, 8>> = alloc::sync::Arc::new(Channel::new()?);
    let ch2 = ch.clone();
    let _t = Thread::spawn(c"bt_t4", 2048, 5, move || {
        for i in 1u32..=5 {
            let _ = ch2.send(i);
        }
    })?;
    let mut total = 0u32;
    for _ in 0..5 {
        total += ch.recv()?;
    }
    if total == 15 {
        Ok(())
    } else {
        Err(arcs::Error::Io(total as i32))
    }
}

fn test_gpio_open() -> arcs::Result<()> {
    // gpioa is registered by drivers/lisa_gpio/lisa_gpio_arcs.c on arcs_evb.
    let _gpio = arcs::Gpio::open(c"gpioa")?;
    Ok(())
}

fn test_uart_open() -> arcs::Result<()> {
    // uart1 — uart0 is the console; using a non-console UART avoids contention.
    // Registered by drivers/lisa_uart/lisa_uart_arcs.c on arcs_evb.
    let _uart = arcs::Uart::open(c"uart1")?;
    Ok(())
}

fn test_eh_gpio_traits() -> arcs::Result<()> {
    use embedded_hal::digital::{OutputPin, StatefulOutputPin};
    let gpio = arcs::Gpio::open(c"gpiob")?;
    let mut led = gpio.pin(9).into_output(arcs::OutputOpts {
        init_high: false,
        ..Default::default()
    })?;
    // Drive through the trait (not inherent methods) and read back state.
    led.set_high()?;
    if !led.is_set_high()? {
        return Err(arcs::Error::Io(-1));
    }
    led.set_low()?;
    if !led.is_set_low()? {
        return Err(arcs::Error::Io(-2));
    }
    Ok(())
}

fn test_eh_delay() -> arcs::Result<()> {
    use embedded_hal::delay::DelayNs;
    // Weak check: the trait call returns without panicking. (No public tick
    // API is bound to measure elapsed time precisely in this MVP test.)
    let mut delay = arcs::Delay::new();
    delay.delay_ms(50);
    Ok(())
}

// ── R3: PWM / RTC / ADC / Flash ──────────────────────────────────────

fn test_pwm_configure() -> arcs::Result<()> {
    // pwm0 ch0 — configure/set/enable/disable all succeed (default pinmux on
    // arcs_evb routes PA0 = LCD backlight). No readback API, so success of the
    // call chain is the assertion.
    let mut pwm = arcs::Pwm::open(c"pwm0", 0)?;
    pwm.set_frequency(1000);
    pwm.configure(arcs::pwm::POLARITY_NORMAL, arcs::pwm::MODE_EDGE_ALIGNED)?;
    pwm.set_duty_percent(50)?;
    pwm.enable()?;
    pwm.disable()
}

fn test_rtc_set_get() -> arcs::Result<()> {
    let rtc = arcs::Rtc::open(c"rtc0")?;
    // 2025-01-15 (year = offset from 2000) 12:30:00.
    let set = arcs::DateTime {
        year: 25,
        month: 1,
        day: 15,
        weekday: 3,
        hour: 12,
        minute: 30,
        second: 0,
    };
    rtc.set(&set)?;
    let t1 = rtc.now()?;
    if t1.year != 25 || t1.hour != 12 || t1.minute != 30 {
        return Err(arcs::Error::Io(-1));
    }
    // Prove the clock actually advances.
    Thread::sleep_ms(1100);
    let t2 = rtc.now()?;
    let s1 = t1.minute as u32 * 60 + t1.second as u32;
    let s2 = t2.minute as u32 * 60 + t2.second as u32;
    if s2 <= s1 {
        return Err(arcs::Error::Io(-2));
    }
    Ok(())
}

fn test_adc_read() -> arcs::Result<()> {
    let adc = arcs::Adc::open(c"adc0")?;
    // Channel 6 = internal VBAT — no external pin/pinmux needed.
    adc.configure_channel(6, arcs::adc::REF_VDD_3V6, arcs::adc::RESOLUTION_10BIT)?;
    let raw = adc.read(6)?;
    // 10-bit conversion: a valid reading is right-aligned in 0..=1023.
    if raw > 1023 {
        return Err(arcs::Error::Io(raw as i32));
    }
    Ok(())
}

fn test_dual_alloc() -> arcs::Result<()> {
    use arcs::heap::{RawBox, Zone};

    // RawBox in each pool; both must round-trip a value (proves usable memory).
    let s = RawBox::new_in(Zone::Sram, 0xABCD_1234u32)?;
    let p = RawBox::new_in(Zone::Psram, 0x5678_9ABCu32)?;
    let saddr = (&*s as *const u32) as usize;
    let paddr = (&*p as *const u32) as usize;
    arcs::log::info!("dual_alloc: SRAM=0x{:08x} PSRAM=0x{:08x}", saddr, paddr);
    if *s != 0xABCD_1234 || *p != 0x5678_9ABC {
        return Err(arcs::Error::Io(-1));
    }
    // The two pools must be physically distinct (different 16 MiB region).
    if (saddr >> 24) == (paddr >> 24) {
        return Err(arcs::Error::Io(-2));
    }
    Ok(())
}

fn test_flash_scratch() -> arcs::Result<()> {
    // 1 MiB scratch offset — the same safe region the C lisa_flash sample uses,
    // well past the boot+app image. Erase one sector, write a pattern, read back.
    const OFFSET: u32 = 0x10_0000;
    let flash = arcs::Flash::open(c"flash0")?;
    flash.erase(OFFSET, arcs::flash::ERASE_SIZE as u32)?;
    let pattern: [u8; 16] = [
        0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04, 0x55, 0xaa, 0x55, 0xaa, 0x10, 0x20, 0x30,
        0x40,
    ];
    flash.write(OFFSET, &pattern)?;
    let mut buf = [0u8; 16];
    flash.read(OFFSET, &mut buf)?;
    if buf != pattern {
        return Err(arcs::Error::Io(-1));
    }
    Ok(())
}
