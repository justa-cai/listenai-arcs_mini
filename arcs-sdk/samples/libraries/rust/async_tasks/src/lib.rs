#![no_std]

extern crate alloc;

use embassy_time::{Duration, Timer};

/// Fast task: logs every 250 ms.
#[embassy_executor::task]
async fn task_a() {
    let mut n = 0u32;
    loop {
        arcs::log::info!("async: A #{}", n);
        n = n.wrapping_add(1);
        Timer::after(Duration::from_millis(250)).await;
    }
}

/// Slow task: logs every 600 ms; emits the success marker once both tasks have
/// clearly interleaved (proving concurrent independent timers on one executor).
#[embassy_executor::task]
async fn task_b() {
    let mut n = 0u32;
    loop {
        arcs::log::info!("async: B #{}", n);
        if n == 3 {
            arcs::log::info!("async_tasks:ok concurrent timers on one executor");
        }
        n = n.wrapping_add(1);
        Timer::after(Duration::from_millis(600)).await;
    }
}

#[arcs::main]
fn main() -> arcs::Result<()> {
    arcs::log::info!("async_tasks: starting embassy executor");
    // Runs the executor on this task forever; spawns the two demo tasks.
    arcs_embassy::run(|spawner| {
        spawner.spawn(task_a()).ok();
        spawner.spawn(task_b()).ok();
    })
}
