include(ExternalProject)
add_dependencies(${LISTENAI_EXECUTABLE_NAME} boot)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(boot_bin_path ${CMAKE_BINARY_DIR}/boot/boot.bin)
set(padded_boot_bin_path ${CMAKE_BINARY_DIR}/boot/boot.padded.bin)
set(boot_source_dir ${CMAKE_CURRENT_LIST_DIR}/boot)
set(app_without_boot_path ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}.bin.without.boot)
set(MERGED_BINARY_PATH ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}_with_boot.bin)
set(FILL_DATA_PATH ${CMAKE_BINARY_DIR}/fill_data.bin)
set(BOOT_RESERVED_TAIL_SIZE 0)

math(EXPR FIRMWARE_SIZE "${CONFIG_BOOT_FLASH_SIZE}")

if(DEFINED CONFIG_BOOT_PARTAB_SIZE)
    set(BOOT_RESERVED_TAIL_SIZE ${CONFIG_BOOT_PARTAB_SIZE})
endif()

if(DEFINED CONFIG_BOOT_CONTROL_STORE_BASE_ADDR AND DEFINED CONFIG_BOOT_CONTROL_STORE_SIZE)
    math(EXPR BOOT_REGION_END "${CONFIG_MEM_FLASH_BASE} + ${CONFIG_BOOT_FLASH_SIZE}")
    math(EXPR CURRENT_TAIL_BASE "${BOOT_REGION_END} - ${BOOT_RESERVED_TAIL_SIZE}")
    math(EXPR CONTROL_STORE_END
        "${CONFIG_BOOT_CONTROL_STORE_BASE_ADDR} + ${CONFIG_BOOT_CONTROL_STORE_SIZE}"
    )

    # When control_store is placed right before the existing boot tail reserve,
    # shrink the boot payload budget as well so boot.bin cannot overwrite it.
    if(CONTROL_STORE_END EQUAL CURRENT_TAIL_BASE)
        math(EXPR BOOT_RESERVED_TAIL_SIZE
            "${BOOT_RESERVED_TAIL_SIZE} + ${CONFIG_BOOT_CONTROL_STORE_SIZE}"
        )
    endif()
endif()

if (NOT DEFINED CONFIG_MEM_CONFIG)
    if (NOT DEFINED CONFIG_BOOT_CP_ENTRY)
        message(FATAL_ERROR "CONFIG_BOOT_CP_ENTRY is not defined")
    endif()
    math(EXPR CP_ENTRY "${CONFIG_BOOT_CP_ENTRY}")
    math(EXPR EXPECT_CP_ENTRY "0x30000000 + ${CONFIG_BOOT_FLASH_SIZE}")
    to_hex(${EXPECT_CP_ENTRY} EXPECT_CP_ENTRY_HEX)

    if (NOT ${CP_ENTRY} STREQUAL ${EXPECT_CP_ENTRY})
        message(FATAL_ERROR "CONFIG_BOOT_CP_ENTRY:${CONFIG_BOOT_CP_ENTRY} is not config correctly, expect entry:${EXPECT_CP_ENTRY_HEX}")
    endif()
else()
    math(EXPR CP_ENTRY "${CONFIG_MEM_FLASH_BASE} + ${CONFIG_BOOT_FLASH_SIZE}")
endif()

to_hex(${CP_ENTRY} CP_ENTRY_HEX)

set(BOOT_CONFIGS "")

set(main_dot_config ${CMAKE_BINARY_DIR}/.config)
if(EXISTS "${main_dot_config}")
    file(
        STRINGS ${main_dot_config}
        BOOT_CONFIG_LINES
        REGEX "^(CONFIG_BOOT_|# CONFIG_BOOT_)"
        ENCODING "UTF-8"
    )

    foreach(line IN LISTS BOOT_CONFIG_LINES)
        if(line MATCHES "^CONFIG_BOOT_HART=" OR line MATCHES "^# CONFIG_BOOT_HART is not set")
            continue()
        endif()
        message(STATUS "boot cfg: ${line}")
        list(APPEND BOOT_CONFIGS "${line}")
    endforeach()
else()
    get_cmake_property(VARS VARIABLES)
    foreach(VAR IN LISTS VARS)
        if(VAR MATCHES "^CONFIG_BOOT_" AND NOT VAR STREQUAL "CONFIG_BOOT_HART")
            message(STATUS "${VAR} = ${${VAR}}")
            list(APPEND BOOT_CONFIGS "${VAR}=${${VAR}}")
        endif()
    endforeach()
endif()

set(BOOT_REQUIRED_CONFIGS
    "CONFIG_BOOT_CP_ENTRY=${CP_ENTRY_HEX}"
)

list(APPEND BOOT_CONFIGS ${BOOT_REQUIRED_CONFIGS})

set(BOOT_APP_CONFIG_FILE ${CMAKE_BINARY_DIR}/boot/app.config)
file(WRITE ${BOOT_APP_CONFIG_FILE} "")
foreach(config IN LISTS BOOT_CONFIGS)
    file(APPEND ${BOOT_APP_CONFIG_FILE} "${config}\n")
endforeach()

set(BOOT_EXTRA_DEFINES "")
if(DEFINED CONFIG_MEM_FLASH_BASE)
    list(APPEND BOOT_EXTRA_DEFINES "CONFIG_MEM_FLASH_BASE=${CONFIG_MEM_FLASH_BASE}")
endif()
string(REPLACE ";" "|" BOOT_EXTRA_DEFINES_STR "${BOOT_EXTRA_DEFINES}")

set(BOOT_DEBUG_CMAKE_ARG "")
if(DEFINED CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES AND CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES)
    set(BOOT_DEBUG_CMAKE_ARG -DCONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES=ON)
endif()

ExternalProject_Add(
    boot
    SOURCE_DIR ${boot_source_dir}
    BINARY_DIR ${CMAKE_BINARY_DIR}/boot
    CMAKE_ARGS
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCHIP=${CHIP}
        -DARCH=${ARCH}
        -DBOARD=${BOARD}
        -DARCS_SDK_BASE=${ARCS_SDK_BASE}
        -DCONFIG_FILES=${BOOT_APP_CONFIG_FILE}
        -DBOOT_EXTRA_DEFINES=${BOOT_EXTRA_DEFINES_STR}
        ${BOOT_DEBUG_CMAKE_ARG}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .
)

set(_boot_app_objcopy_args -S)
if(LISTENAI_EX_SECTIONS)
    list(APPEND _boot_app_objcopy_args -R ${LISTENAI_EX_SECTIONS})
endif()
list(APPEND _boot_app_objcopy_args
    -O binary
    ${LISTENAI_EXECUTABLE_NAME}
    ${app_without_boot_path}
)

set(_boot_app_mkhdr_flags_arg "")
if(NOT DEFINED LISTENAI_TOOLS_MKHDR_COMMAND)
    set(LISTENAI_TOOLS_MKHDR_COMMAND "${LISTENAI_TOOLS_MKHDR}")
endif()
if(LISTENAI_MKHDR_TARGET_CORE AND DEFINED CONFIG_HARTID)
    execute_process(
        COMMAND ${LISTENAI_TOOLS_MKHDR_COMMAND} -h
        OUTPUT_VARIABLE _boot_app_mkhdr_help
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_boot_app_mkhdr_help MATCHES "-f ")
        set(_boot_app_mkhdr_flags_arg -f ${CONFIG_HARTID})
    else()
        message(WARNING "mkhdr does not support -f (target_core); update mkhdr to write boot core into image header")
    endif()
endif()

# Regenerate the app-only payload from the ELF before every merge. The final
# merged firmware also uses <name>.bin, so never use that path as app input.
add_custom_target(
    boot_app_mkhdr
    ALL
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating app-only binary: ${LISTENAI_EXECUTABLE_NAME}.bin.without.boot"
    COMMAND ${CMAKE_OBJCOPY} ${_boot_app_objcopy_args}
    COMMAND ${CMAKE_COMMAND} -E echo "-- Generating ListenAI Boot Header for ${LISTENAI_EXECUTABLE_NAME}.bin.without.boot"
    COMMAND ${LISTENAI_TOOLS_MKHDR_COMMAND} ${_boot_app_mkhdr_flags_arg} ${app_without_boot_path}
    DEPENDS ${LISTENAI_EXECUTABLE_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set(_boot_merge_deps boot boot_app_mkhdr)
if(TARGET mkhdr)
    list(APPEND _boot_merge_deps mkhdr)
endif()

add_custom_target(
    merge_boot
    ALL
    COMMAND ${CMAKE_COMMAND}
        -DBOOT_BIN=${boot_bin_path}
        -DBOOT_FLASH_SIZE=${FIRMWARE_SIZE}
        -DBOOT_RESERVED_TAIL_SIZE=${BOOT_RESERVED_TAIL_SIZE}
        -P ${CMAKE_CURRENT_LIST_DIR}/check_boot_image_size.cmake
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_LIST_DIR}/pad_binary.py
        --input ${boot_bin_path}
        --output ${padded_boot_bin_path}
        --size ${FIRMWARE_SIZE}
        --fill-byte 0xFF
    COMMAND ${CMAKE_COMMAND} -E echo "-- Merging boot and app binary files"
    COMMAND ${CMAKE_COMMAND} -E cat ${padded_boot_bin_path} ${app_without_boot_path} > ${MERGED_BINARY_PATH}
    COMMAND ${CMAKE_COMMAND} -E rename ${MERGED_BINARY_PATH} ${LISTENAI_EXECUTABLE_NAME}.bin

    DEPENDS ${_boot_merge_deps}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
