//! Procedural macros for the `arcs` crate.
//!
//! Host-compiled (proc-macro crates always build for the build machine), so
//! `std` is available here and nothing in this crate reaches the firmware.

use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, ItemFn};

/// Attribute macro turning an idiomatic `fn main() -> arcs::Result<()>` into
/// the C-callable entry the SDK expects.
///
/// Injects the global allocator, panic handler, and one-time runtime init —
/// equivalent to `arcs::entry!` but with attribute syntax and a fixed export
/// symbol `rust_main`. The C side calls `extern int rust_main(void);`.
///
/// # Constraint
///
/// Exactly one `#[arcs::main]` (or one `arcs::entry!`) per crate:
/// `#[global_allocator]`/`#[panic_handler]` are crate-root singletons.
#[proc_macro_attribute]
pub fn main(_attr: TokenStream, item: TokenStream) -> TokenStream {
    let func = parse_macro_input!(item as ItemFn);
    let block = &func.block;
    let expanded = quote! {
        #[global_allocator]
        static __ARCS_GLOBAL_ALLOC: ::arcs::heap::ArcsAllocator = ::arcs::heap::ArcsAllocator;

        #[panic_handler]
        fn __arcs_panic_handler(info: &core::panic::PanicInfo) -> ! {
            ::arcs::__private::panic_impl(info)
        }

        #[no_mangle]
        pub extern "C" fn rust_main() -> core::ffi::c_int {
            ::arcs::__private::entry_init();
            let __result: ::arcs::Result<()> = (|| -> ::arcs::Result<()> { #block })();
            match __result {
                Ok(())  => 0,
                Err(e)  => {
                    ::log::error!("rust main failed: {:?}", e);
                    -1
                }
            }
        }
    };
    expanded.into()
}
