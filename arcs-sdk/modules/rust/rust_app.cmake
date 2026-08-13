# Shared CMake helpers for building ARCS Rust staticlib crates.
#
# Callers choose the Cargo workspace, crate directory, target triple, features,
# and any app-specific glue. This file only handles the common Cargo build,
# imported static library, and whole-archive link wiring.

include_guard(GLOBAL)

function(arcs_rust_resolve_dir OUT_VAR)
    if(DEFINED ARCS_RUST_DIR AND NOT "${ARCS_RUST_DIR}" STREQUAL "")
        set(_arcs_rust_dir "${ARCS_RUST_DIR}")
    elseif(DEFINED ENV{ARCS_RUST_DIR} AND NOT "$ENV{ARCS_RUST_DIR}" STREQUAL "")
        set(_arcs_rust_dir "$ENV{ARCS_RUST_DIR}")
    elseif(DEFINED CMAKE_CURRENT_FUNCTION_LIST_DIR
            AND EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Cargo.toml")
        set(_arcs_rust_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    elseif(DEFINED ENV{ARCS_BASE} AND NOT "$ENV{ARCS_BASE}" STREQUAL "")
        set(_arcs_rust_dir "$ENV{ARCS_BASE}/modules/rust")
    else()
        set(_arcs_rust_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    endif()

    get_filename_component(_arcs_rust_dir "${_arcs_rust_dir}" ABSOLUTE)
    if(NOT EXISTS "${_arcs_rust_dir}/Cargo.toml")
        message(FATAL_ERROR
            "ARCS_RUST_DIR=${_arcs_rust_dir} does not contain Cargo.toml")
    endif()

    set(ARCS_RUST_DIR "${_arcs_rust_dir}" CACHE PATH
        "Root of the ARCS SDK Rust workspace/helper directory")
    set(${OUT_VAR} "${_arcs_rust_dir}" PARENT_SCOPE)
endfunction()

function(arcs_rust_find_cargo OUT_VAR)
    if(NOT CARGO_BIN)
        if(DEFINED ENV{CARGO} AND NOT "$ENV{CARGO}" STREQUAL ""
                AND EXISTS "$ENV{CARGO}")
            set(CARGO_BIN "$ENV{CARGO}" CACHE FILEPATH "Cargo executable")
        else()
            find_program(CARGO_BIN cargo
                HINTS
                    /usr/local/opt/rustup/bin
                    /opt/homebrew/opt/rustup/bin
                    $ENV{HOME}/.cargo/bin)
        endif()
        if(NOT CARGO_BIN)
            message(FATAL_ERROR
                "cargo not found.\n"
                "Install rustup via https://rustup.rs (or `brew install rustup`).")
        endif()
    endif()
    set(${OUT_VAR} "${CARGO_BIN}" PARENT_SCOPE)
endfunction()

function(arcs_rust_parse_package_name OUT_VAR CRATE_DIR)
    set(_cargo_toml "${CRATE_DIR}/Cargo.toml")
    if(NOT EXISTS "${_cargo_toml}")
        message(FATAL_ERROR "Rust crate directory has no Cargo.toml: ${CRATE_DIR}")
    endif()

    file(READ "${_cargo_toml}" _toml_text)
    if(_toml_text MATCHES "name[ \t]*=[ \t]*\"([^\"]+)\"")
        set(${OUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Could not parse [package].name from ${_cargo_toml}")
    endif()
endfunction()

function(arcs_rust_toml_literal_path OUT_VAR INPUT_PATH)
    get_filename_component(_path_abs "${INPUT_PATH}" ABSOLUTE)
    file(TO_CMAKE_PATH "${_path_abs}" _path_toml)
    if(_path_toml MATCHES "'")
        message(FATAL_ERROR
            "Cargo --config path contains a single quote and cannot be encoded: ${_path_toml}")
    endif()
    set(${OUT_VAR} "'${_path_toml}'" PARENT_SCOPE)
endfunction()

function(arcs_rust_add_glue_sources TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR
            "arcs_rust_add_glue_sources target does not exist: ${TARGET}")
    endif()
    if(NOT TARGET arcs-rust-glue)
        message(FATAL_ERROR
            "arcs_rust_add_glue_sources requires the arcs-rust-glue target")
    endif()

    get_target_property(_glue_sources arcs-rust-glue SOURCES)
    if(_glue_sources)
        target_sources(${TARGET} PRIVATE ${_glue_sources})
    endif()

    foreach(_prop IN ITEMS INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(_glue_include_dirs arcs-rust-glue ${_prop})
        if(_glue_include_dirs)
            target_include_directories(${TARGET} PRIVATE ${_glue_include_dirs})
        endif()
    endforeach()

    foreach(_prop IN ITEMS COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
        get_target_property(_glue_compile_defs arcs-rust-glue ${_prop})
        if(_glue_compile_defs)
            target_compile_definitions(${TARGET} PRIVATE ${_glue_compile_defs})
        endif()
    endforeach()

    foreach(_prop IN ITEMS COMPILE_OPTIONS INTERFACE_COMPILE_OPTIONS)
        get_target_property(_glue_compile_opts arcs-rust-glue ${_prop})
        if(_glue_compile_opts)
            target_compile_options(${TARGET} PRIVATE ${_glue_compile_opts})
        endif()
    endforeach()

    foreach(_prop IN ITEMS COMPILE_FEATURES INTERFACE_COMPILE_FEATURES)
        get_target_property(_glue_compile_features arcs-rust-glue ${_prop})
        if(_glue_compile_features)
            target_compile_features(${TARGET} PRIVATE ${_glue_compile_features})
        endif()
    endforeach()

    foreach(_prop IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_glue_link_libraries arcs-rust-glue ${_prop})
        if(_glue_link_libraries)
            target_link_libraries(${TARGET} PRIVATE ${_glue_link_libraries})
        endif()
    endforeach()
endfunction()

function(arcs_rust_link_staticlib TARGET)
    cmake_parse_arguments(RUST_APP
        ""
        "CRATE_DIR;WORKSPACE_DIR;TOOLCHAIN_DIR;PACKAGE;TARGET_TRIPLE;OUT_DIR;PROFILE;PROFILE_DIR"
        "FEATURES;ENV;DEPENDS;DEP_CRATE_DIRS;PATCH_CRATE_DIRS;CONFIG_PATHS"
        ${ARGN})
    if(RUST_APP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unknown arcs_rust_link_staticlib arguments: ${RUST_APP_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT RUST_APP_CRATE_DIR)
        message(FATAL_ERROR "arcs_rust_link_staticlib requires CRATE_DIR")
    endif()
    if(NOT RUST_APP_WORKSPACE_DIR)
        message(FATAL_ERROR "arcs_rust_link_staticlib requires WORKSPACE_DIR")
    endif()
    if(NOT RUST_APP_TARGET_TRIPLE)
        message(FATAL_ERROR "arcs_rust_link_staticlib requires TARGET_TRIPLE")
    endif()

    get_filename_component(_crate_dir "${RUST_APP_CRATE_DIR}" ABSOLUTE)
    get_filename_component(_workspace_dir "${RUST_APP_WORKSPACE_DIR}" ABSOLUTE)
    if(RUST_APP_TOOLCHAIN_DIR)
        get_filename_component(_toolchain_dir "${RUST_APP_TOOLCHAIN_DIR}" ABSOLUTE)
    else()
        set(_toolchain_dir "${_workspace_dir}")
    endif()
    if(RUST_APP_PACKAGE)
        set(_pkg "${RUST_APP_PACKAGE}")
    else()
        arcs_rust_parse_package_name(_pkg "${_crate_dir}")
    endif()
    string(REPLACE "-" "_" _lib_base "${_pkg}")

    if(RUST_APP_OUT_DIR)
        set(_out_dir "${RUST_APP_OUT_DIR}")
    else()
        set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/cargo-out")
    endif()
    if(RUST_APP_PROFILE)
        set(_profile "${RUST_APP_PROFILE}")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_profile dev)
    else()
        set(_profile release)
    endif()
    if(RUST_APP_PROFILE_DIR)
        set(_profile_dir "${RUST_APP_PROFILE_DIR}")
    elseif(_profile STREQUAL "dev")
        set(_profile_dir debug)
    else()
        set(_profile_dir "${_profile}")
    endif()

    set(_lib_out
        "${_out_dir}/${RUST_APP_TARGET_TRIPLE}/${_profile_dir}/lib${_lib_base}.a")

    arcs_rust_resolve_dir(_arcs_rust_dir)
    arcs_rust_find_cargo(_cargo_bin)
    set(_cargo_stdin_null "${_arcs_rust_dir}/cargo_stdin_null.sh")

    file(GLOB_RECURSE _rust_inputs CONFIGURE_DEPENDS
        "${_crate_dir}/src/*.rs"
        "${_crate_dir}/Cargo.toml"
        "${_crate_dir}/build.rs"
        "${_crate_dir}/glue/*.c"
        "${_workspace_dir}/Cargo.toml"
        "${_workspace_dir}/Cargo.lock"
        "${_workspace_dir}/rust-toolchain.toml"
        "${_workspace_dir}/.cargo/config.toml")
    foreach(_toolchain_input
            "${_toolchain_dir}/rust-toolchain.toml"
            "${_toolchain_dir}/.cargo/config.toml")
        if(EXISTS "${_toolchain_input}")
            list(APPEND _rust_inputs "${_toolchain_input}")
        endif()
    endforeach()

    set(_dep_crate_dirs ${RUST_APP_DEP_CRATE_DIRS} ${RUST_APP_PATCH_CRATE_DIRS})
    if(_dep_crate_dirs)
        list(REMOVE_DUPLICATES _dep_crate_dirs)
    endif()

    foreach(_dep_crate IN LISTS _dep_crate_dirs)
        get_filename_component(_dep_crate_abs "${_dep_crate}" ABSOLUTE)
        file(GLOB_RECURSE _dep_inputs CONFIGURE_DEPENDS
            "${_dep_crate_abs}/src/*.rs"
            "${_dep_crate_abs}/Cargo.toml"
            "${_dep_crate_abs}/build.rs"
            "${_dep_crate_abs}/glue/*.c")
        list(APPEND _rust_inputs ${_dep_inputs})
    endforeach()
    list(APPEND _rust_inputs ${RUST_APP_DEPENDS} "${_cargo_stdin_null}")

    set(_cargo_config_args "")
    foreach(_config_path IN LISTS RUST_APP_CONFIG_PATHS)
        get_filename_component(_config_abs "${_config_path}" ABSOLUTE)
        if(NOT EXISTS "${_config_abs}")
            message(FATAL_ERROR "Cargo config path does not exist: ${_config_abs}")
        endif()
        list(APPEND _cargo_config_args --config "${_config_abs}")
        list(APPEND _rust_inputs "${_config_abs}")
    endforeach()

    if(RUST_APP_PATCH_CRATE_DIRS)
        string(MAKE_C_IDENTIFIER "${TARGET}" _target_id)
        set(_patch_config_path
            "${CMAKE_CURRENT_BINARY_DIR}/${_target_id}_cargo_patch_config.toml")
        file(WRITE "${_patch_config_path}" "[patch.crates-io]\n")

        foreach(_patch_crate IN LISTS RUST_APP_PATCH_CRATE_DIRS)
            get_filename_component(_patch_crate_abs "${_patch_crate}" ABSOLUTE)
            arcs_rust_parse_package_name(_patch_pkg "${_patch_crate_abs}")
            arcs_rust_toml_literal_path(_patch_path "${_patch_crate_abs}")
            file(APPEND "${_patch_config_path}"
                "${_patch_pkg} = { path = ${_patch_path} }\n")
            message(STATUS
                "Rust crate patch for ${_pkg}: ${_patch_pkg} -> ${_patch_crate_abs}")
        endforeach()

        list(APPEND _cargo_config_args --config "${_patch_config_path}")
        list(APPEND _rust_inputs "${_patch_config_path}")
    endif()

    set(_feature_args "")
    if(RUST_APP_FEATURES)
        set(_features ${RUST_APP_FEATURES})
        list(REMOVE_DUPLICATES _features)
        list(JOIN _features "," _feature_csv)
        set(_feature_args --features ${_feature_csv})
        message(STATUS "Rust features for ${_pkg}: ${_feature_csv}")
    endif()

    get_filename_component(_cargo_dir "${_cargo_bin}" DIRECTORY)
    find_program(_rustup_bin rustup HINTS "${_cargo_dir}" NO_DEFAULT_PATH)
    set(_rustc_env "")
    set(_unset_rustc --unset=RUSTC)
    if(_rustup_bin)
        execute_process(
            COMMAND "${_rustup_bin}" which rustc
            WORKING_DIRECTORY "${_toolchain_dir}"
            OUTPUT_VARIABLE _rustup_rustc
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _rustup_rc)
        if(_rustup_rc EQUAL 0 AND _rustup_rustc)
            set(_rustc_env RUSTC=${_rustup_rustc})
            set(_unset_rustc "")
            message(STATUS "Rust workspace rustc for ${_pkg}: ${_rustup_rustc}")
        endif()
    endif()

    if(CMAKE_HOST_WIN32)
        set(_rustc_wrapper_env "")
        set(_cargo_build_command "${_cargo_bin}")
    else()
        set(_rustc_wrapper_env RUSTC_WRAPPER=${_cargo_stdin_null})
        set(_cargo_build_command /bin/sh "${_cargo_stdin_null}" "${_cargo_bin}")
    endif()

    add_custom_command(
        OUTPUT "${_lib_out}"
        COMMAND ${CMAKE_COMMAND} -E env
                CARGO_TARGET_DIR=${_out_dir}
                ${RUST_APP_ENV}
                ${_rustc_env}
                ${_unset_rustc}
                --unset=RUSTC_WRAPPER --unset=RUSTC_WORKSPACE_WRAPPER
                --unset=CARGO_BUILD_RUSTC_WRAPPER --unset=CARGO_BUILD_RUSTC
                --unset=RUSTFLAGS --unset=CARGO_ENCODED_RUSTFLAGS
                --unset=CARGO_BUILD_RUSTFLAGS
                ${_rustc_wrapper_env}
                --
                ${_cargo_build_command} ${_cargo_config_args}
                    build --manifest-path "${_crate_dir}/Cargo.toml" --profile ${_profile}
                    --target ${RUST_APP_TARGET_TRIPLE} -p ${_pkg} ${_feature_args}
        WORKING_DIRECTORY "${_toolchain_dir}"
        DEPENDS ${_rust_inputs}
        COMMENT "Building Rust crate ${_pkg} (${_profile}, ${RUST_APP_TARGET_TRIPLE})")

    set(_build_target "${TARGET}-rust-app-build")
    add_custom_target(${_build_target} DEPENDS "${_lib_out}")

    set(_imported_target "${TARGET}-rust-app")
    add_library(${_imported_target} STATIC IMPORTED)
    set_target_properties(${_imported_target} PROPERTIES
        IMPORTED_LOCATION "${_lib_out}")
    add_dependencies(${_imported_target} ${_build_target})

    target_link_options(${TARGET} PRIVATE
        -Wl,--whole-archive $<TARGET_FILE:${_imported_target}> -Wl,--no-whole-archive)
    add_dependencies(${TARGET} ${_imported_target})

    message(STATUS
        "Rust crate wired: ${TARGET} <- ${_pkg} (lib${_lib_base}.a, ${RUST_APP_TARGET_TRIPLE})")
endfunction()
