# rust_sample.cmake - SDK Rust sample wrapper.
#
# Usage (in a sample's CMakeLists.txt):
#   set(LISTENAI_RUST_APP ${CMAKE_CURRENT_SOURCE_DIR})  # dir holding Cargo.toml
#   listenai_add_executable(${PROJECT_NAME})
#   include($ENV{ARCS_BASE}/modules/rust/rust_sample.cmake)
#   listenai_maybe_enable_rust_target(${PROJECT_NAME})
#   target_sources(${PROJECT_NAME} PRIVATE src/main.c)
#
# The selected CMake project is still controlled by build.sh -S. Samples are
# standalone Cargo packages; this wrapper injects SDK crate paths at build time
# so the manifests do not depend on a fixed relative modules/rust location.

include("${CMAKE_CURRENT_LIST_DIR}/rust_app.cmake")

function(listenai_maybe_enable_rust_target TARGET)
    cmake_parse_arguments(RUST_TGT "" "CRATE_DIR;WORKSPACE_DIR" "FEATURES" ${ARGN})
    if(RUST_TGT_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown listenai_maybe_enable_rust_target arguments: ${RUST_TGT_UNPARSED_ARGUMENTS}")
    endif()
    if(RUST_TGT_CRATE_DIR)
        set(LISTENAI_RUST_APP ${RUST_TGT_CRATE_DIR})
    endif()
    if(NOT DEFINED LISTENAI_RUST_APP)
        return()
    endif()

    arcs_rust_resolve_dir(_arcs_rust_dir)

    # Once-guard the shared adapter build (cargo discovery + C glue).
    if(NOT TARGET arcs-rust-glue)
        add_subdirectory("${_arcs_rust_dir}" modules-rust)
    endif()

    set(_sample_cargo_toml "${LISTENAI_RUST_APP}/Cargo.toml")
    if(NOT EXISTS "${_sample_cargo_toml}")
        message(FATAL_ERROR
            "LISTENAI_RUST_APP=${LISTENAI_RUST_APP} but no Cargo.toml there.")
    endif()
    file(READ "${_sample_cargo_toml}" _toml_text)
    if(RUST_TGT_WORKSPACE_DIR)
        set(_cargo_workspace_dir "${RUST_TGT_WORKSPACE_DIR}")
    else()
        set(_cargo_workspace_dir "${LISTENAI_RUST_APP}")
    endif()

    set(_cargo_toolchain_dir "${_arcs_rust_dir}")
    set(_cargo_config_paths "")
    if(EXISTS "${LISTENAI_RUST_APP}/.cargo/config.toml")
        list(APPEND _cargo_config_paths "${LISTENAI_RUST_APP}/.cargo/config.toml")
    else()
        list(APPEND _cargo_config_paths "${_arcs_rust_dir}/.cargo/config.toml")
    endif()
    if(EXISTS "${LISTENAI_RUST_APP}/rust-toolchain.toml")
        # Special samples such as fpu_hardfloat carry their own nightly
        # toolchain/build-std config and must not inherit the stable toolchain.
        set(_cargo_toolchain_dir "${LISTENAI_RUST_APP}")
    endif()

    if(CONFIG_FPU)
        set(_rust_target riscv32imafc-unknown-none-elf)
    else()
        set(_rust_target riscv32imac-unknown-none-elf)
    endif()

    set(_sdk_rust_dep_crates
        "${_arcs_rust_dir}/arcs"
        "${_arcs_rust_dir}/arcs-macros"
        "${_arcs_rust_dir}/arcs-embassy")
    set(_sdk_rust_patch_crates "")
    if(_toml_text MATCHES "(^|\n)[ \t]*arcs[ \t]*=")
        list(APPEND _sdk_rust_patch_crates "${_arcs_rust_dir}/arcs")
    endif()
    if(_toml_text MATCHES "(^|\n)[ \t]*arcs-macros[ \t]*=")
        list(APPEND _sdk_rust_patch_crates "${_arcs_rust_dir}/arcs-macros")
    endif()
    if(_toml_text MATCHES "(^|\n)[ \t]*arcs-embassy[ \t]*=")
        list(APPEND _sdk_rust_patch_crates "${_arcs_rust_dir}/arcs-embassy")
    endif()

    arcs_rust_link_staticlib(${TARGET}
        CRATE_DIR "${LISTENAI_RUST_APP}"
        WORKSPACE_DIR "${_cargo_workspace_dir}"
        TOOLCHAIN_DIR "${_cargo_toolchain_dir}"
        TARGET_TRIPLE "${_rust_target}"
        FEATURES ${RUST_TGT_FEATURES}
        DEP_CRATE_DIRS ${_sdk_rust_dep_crates}
        PATCH_CRATE_DIRS ${_sdk_rust_patch_crates}
        CONFIG_PATHS ${_cargo_config_paths})

    # Some SDK link paths turn OBJECT libraries into -l flags. Reuse the
    # arcs-rust-glue target's sources and usage requirements without linking
    # the OBJECT target itself.
    arcs_rust_add_glue_sources(${TARGET})

    message(STATUS
        "Rust sample wired: ${TARGET} (manifest: ${LISTENAI_RUST_APP}, toolchain: ${_cargo_toolchain_dir})")
endfunction()
