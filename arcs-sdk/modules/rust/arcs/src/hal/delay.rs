//! Delay provider implementing embedded-hal's DelayNs.

#[cfg(feature = "embedded-hal")]
use crate::Thread;

/// Zero-sized delay handle. Millisecond delays are backed by the RTOS
/// scheduler (`Thread::sleep_ms`, which yields the CPU). Sub-millisecond
/// requests round up to a single 1 ms tick — precise sub-ms timing
/// (cycle-counter busy-wait) is a future refinement.
#[derive(Clone, Copy, Default)]
pub struct Delay;

impl Delay {
    pub fn new() -> Self {
        Delay
    }
}

#[cfg(feature = "embedded-hal")]
impl embedded_hal::delay::DelayNs for Delay {
    fn delay_ns(&mut self, ns: u32) {
        let ms = ns.div_ceil(1_000_000).max(1);
        Thread::sleep_ms(ms);
    }
    fn delay_ms(&mut self, ms: u32) {
        Thread::sleep_ms(ms);
    }
}
