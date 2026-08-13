#![no_std]

extern crate alloc;

arcs::entry!(rust_hello_main, {
    arcs::log::info!("Hello from Rust on ARCS SDK!");
    let v: alloc::vec::Vec<u32> = alloc::vec![1, 2, 3];
    arcs::log::info!("alloc test: sum={}", v.iter().sum::<u32>());
    Ok(())
});
