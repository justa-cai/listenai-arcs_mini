//! Embassy async runtime integration for the ARCS SDK.
//!
//! Brings up an [`embassy_executor`] executor that cooperates with FreeRTOS:
//! the executor runs on the calling FreeRTOS task and, when idle, **blocks on a
//! task notification** (rather than `wfi`) so other RTOS tasks keep running.
//! An [`embassy_time`] driver is provided, backed by the SoC's 64-bit 1 MHz
//! monotonic timer.
//!
//! This crate provides the three link-level singletons embassy needs — the
//! `critical-section` impl, the executor `__pender`, and the `embassy-time`
//! `Driver` — so a binary links it exactly once.
//!
//! The host application must compile the companion C shim
//! `glue/arcs_embassy_glue.c` (it exposes the FreeRTOS macros/inlines this
//! crate calls). Add it to the sample's `target_sources`.
//!
//! ## Usage
//! ```ignore
//! #[embassy_executor::task]
//! async fn blink() { loop { embassy_time::Timer::after_millis(250).await; } }
//!
//! #[arcs::main]
//! fn main() -> arcs::Result<()> {
//!     arcs_embassy::run(|spawner| { spawner.spawn(blink()).ok(); })  // never returns
//! }
//! ```
//! The crate using `#[embassy_executor::task]` must depend on `embassy-executor`
//! and `embassy-time` directly (the task macro emits `::embassy_executor` paths).

#![no_std]

use core::cell::RefCell;
use core::ffi::c_void;
use core::task::Waker;

use critical_section::Mutex;
use embassy_executor::raw;
use embassy_time_queue_utils::Queue;

pub use embassy_executor::Spawner;

extern "C" {
    fn arcs_embassy_now_us() -> u64;
    fn arcs_embassy_cs_acquire() -> u8;
    fn arcs_embassy_cs_release(state: u8);
    fn arcs_embassy_cur_task() -> *mut c_void;
    fn arcs_embassy_notify(task: *mut c_void);
    fn arcs_embassy_notify_take(clear: u32, ticks: u32) -> u32;
}

// FreeRTOS portMAX_DELAY (block "forever").
const PORT_MAX_DELAY: u32 = 0xFFFF_FFFF;

// ── critical-section: raise/restore the FreeRTOS interrupt mask ──────────
struct FreeRtosCs;
critical_section::set_impl!(FreeRtosCs);

unsafe impl critical_section::Impl for FreeRtosCs {
    unsafe fn acquire() -> u8 {
        unsafe { arcs_embassy_cs_acquire() }
    }
    unsafe fn release(state: u8) {
        unsafe { arcs_embassy_cs_release(state) }
    }
}

// ── executor pender: wake the executor task ──────────────────────────────
// `context` is the FreeRTOS task handle passed to `raw::Executor::new`.
// In this single-executor design every wake originates on the executor task
// itself (poll + timer dispatch), so notifying that task is always correct.
#[export_name = "__pender"]
fn __pender(context: *mut ()) {
    unsafe { arcs_embassy_notify(context as *mut c_void) }
}

// ── embassy-time driver: 1 MHz monotonic, queue-backed wakes ─────────────
struct ArcsTimeDriver {
    queue: Mutex<RefCell<Queue>>,
}

embassy_time_driver::time_driver_impl!(
    static DRIVER: ArcsTimeDriver = ArcsTimeDriver {
        queue: Mutex::new(RefCell::new(Queue::new()))
    }
);

impl embassy_time_driver::Driver for ArcsTimeDriver {
    fn now(&self) -> u64 {
        unsafe { arcs_embassy_now_us() }
    }

    fn schedule_wake(&self, at: u64, waker: &Waker) {
        critical_section::with(|cs| {
            self.queue.borrow(cs).borrow_mut().schedule_wake(at, waker);
        });
    }
}

impl ArcsTimeDriver {
    /// Wake every timer whose deadline has passed and return the next pending
    /// deadline in µs (`u64::MAX` if no timers remain).
    fn dispatch(&self) -> u64 {
        let now = unsafe { arcs_embassy_now_us() };
        critical_section::with(|cs| self.queue.borrow(cs).borrow_mut().next_expiration(now))
    }
}

/// Convert a future µs deadline into a FreeRTOS block timeout in 1 ms ticks.
fn ticks_until(next_us: u64) -> u32 {
    if next_us == u64::MAX {
        return PORT_MAX_DELAY;
    }
    let now = unsafe { arcs_embassy_now_us() };
    if next_us <= now {
        return 0;
    }
    // round up µs → ms, clamp below portMAX_DELAY (reserved for "forever").
    let ms = (next_us - now).div_ceil(1_000);
    if ms >= PORT_MAX_DELAY as u64 {
        PORT_MAX_DELAY - 1
    } else {
        ms as u32
    }
}

/// Run the embassy executor on the **current** FreeRTOS task. Never returns;
/// the calling task becomes the executor. `init` spawns the initial tasks.
pub fn run(init: impl FnOnce(Spawner)) -> ! {
    static EXECUTOR: static_cell::StaticCell<raw::Executor> = static_cell::StaticCell::new();

    let task = unsafe { arcs_embassy_cur_task() };
    let executor = EXECUTOR.init(raw::Executor::new(task as *mut ()));
    init(executor.spawner());

    loop {
        // SAFETY: the executor is a 'static singleton, polled only from here.
        unsafe { executor.poll() };
        // Fire any elapsed timers (their wakers re-pend this task), then sleep
        // until the next deadline or the next wake notification.
        let next = DRIVER.dispatch();
        let timeout = ticks_until(next);
        unsafe { arcs_embassy_notify_take(1, timeout) };
    }
}

/// Async delay implementing [`embedded_hal_async::delay::DelayNs`], backed by
/// `embassy_time`.
#[derive(Clone, Copy, Default)]
pub struct Delay;

impl embedded_hal_async::delay::DelayNs for Delay {
    async fn delay_ns(&mut self, ns: u32) {
        // The driver resolves to 1 µs; round sub-µs up.
        embassy_time::Timer::after(embassy_time::Duration::from_micros(
            u64::from(ns).div_ceil(1_000),
        ))
        .await;
    }
    async fn delay_ms(&mut self, ms: u32) {
        embassy_time::Timer::after(embassy_time::Duration::from_millis(u64::from(ms))).await;
    }
}
