//! Macro definitions exported at crate root via #[macro_export].

/// Inject panic handler, global allocator, log init, and a `#[no_mangle]`
/// entry function wrapping the user body.
///
/// # Usage
///
/// ```ignore
/// #![no_std]
/// extern crate alloc;
///
/// arcs::entry!(rust_hello_main, {
///     arcs::log::info!("Hello!");
///     Ok(())
/// });
/// ```
///
/// The body must evaluate to `arcs::Result<()>`. The macro:
///
/// - Installs `arcs::heap::ArcsAllocator` as `#[global_allocator]`
/// - Installs a `#[panic_handler]` that logs to easylogger and traps
/// - Calls `arcs::__private::entry_init()` once (sets up `log` facade)
/// - Maps the result: Ok → return 0; Err → log error and return -1
///
/// The exported symbol is `<name>` (no `_main` suffix appended) — the macro
/// uses whatever you pass verbatim, so C code calling `extern int <name>(void)`
/// must use the exact same identifier.
///
/// # Constraint: invoke exactly once per crate
///
/// `#[global_allocator]` and `#[panic_handler]` are crate-root attributes
/// that may appear at most once. Invoking `arcs::entry!` more than once in
/// the same crate causes a hard compile error ("duplicate panic_handler").
/// If you need multiple Rust-callable entry points from the same sample,
/// add additional `#[no_mangle] pub extern "C" fn ...` items by hand and
/// have them share the macro's panic/allocator infrastructure.
#[macro_export]
macro_rules! entry {
    ($name:ident, $body:block) => {
        #[global_allocator]
        static __ARCS_GLOBAL_ALLOC: $crate::heap::ArcsAllocator = $crate::heap::ArcsAllocator;

        #[panic_handler]
        fn __arcs_panic_handler(info: &core::panic::PanicInfo) -> ! {
            $crate::__private::panic_impl(info)
        }

        #[no_mangle]
        pub extern "C" fn $name() -> core::ffi::c_int {
            $crate::__private::entry_init();
            // Wrap $body in an extra `{ ... }`: a `block` macro fragment
            // cannot be placed directly as a closure body (rustc parser
            // limitation), but is fine inside another block.
            let __result: $crate::Result<()> = (|| -> $crate::Result<()> { $body })();
            match __result {
                Ok(()) => 0,
                Err(e) => {
                    ::log::error!("rust entry [{}] failed: {:?}", stringify!($name), e,);
                    -1
                }
            }
        }
    };
}
