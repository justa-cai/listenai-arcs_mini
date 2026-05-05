include(ExternalProject)
add_dependencies(${LISTENAI_EXECUTABLE_NAME} boot)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# 允许外部指定生成 app.bin 的 target，默认为 mkhdr
if(NOT DEFINED LISTENAI_BOOT_APP_BIN_TARGET)
    set(LISTENAI_BOOT_APP_BIN_TARGET "mkhdr")
endif()

set(boot_bin_path ${CMAKE_BINARY_DIR}/boot/boot.bin)
set(padded_boot_bin_path ${CMAKE_BINARY_DIR}/boot/boot.padded.bin)
set(boot_source_dir ${CMAKE_CURRENT_LIST_DIR}/uboot)
set(app_bin_path ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}.bin)
set(app_without_boot_path ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}.bin.without.boot)
set(MERGED_BINARY_PATH ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}_with_boot.bin)
set(FILL_DATA_PATH ${CMAKE_BINARY_DIR}/fill_data.bin)
set(BOOT_RESERVED_TAIL_SIZE 0)

math(EXPR FIRMWARE_SIZE "${CONFIG_BOOT_FLASH_SIZE}")

if(DEFINED CONFIG_BOOT_PARTAB_SIZE)
    set(BOOT_RESERVED_TAIL_SIZE ${CONFIG_BOOT_PARTAB_SIZE})
elseif(DEFINED CONFIG_BOOT_OTA_PACKAGE)
    set(BOOT_RESERVED_TAIL_SIZE 0x1000)
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
    "CONFIG_APPLICATION_ADDRESS=${CP_ENTRY_HEX}"
    "CONFIG_BOOT_CP_ENTRY=${CP_ENTRY_HEX}"
    "CONFIG_ARCS_AP_CORE=y"
    "# CONFIG_LINK_OPTION_LISTENAI_LIBRARY_WHOLE_ARCHIVE is not set"
    "CONFIG_LINK_OPTION_LISTENAI_LIBRARY_GROUP=y"
    "CONFIG_BOOT_EARLY_INIT=y"
    "CONFIG_BOOT_EARLY_CLOCK_INIT=y"
    "CONFIG_BOOT_EARLY_PSRAM_INIT=y"
)

if(DEFINED CONFIG_BOOT_ADB OR DEFINED CONFIG_BOOT_ADB_SYNC)
    if(DEFINED CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        list(APPEND BOOT_REQUIRED_CONFIGS
            "CONFIG_CHERRYUSB=y"
            "CONFIG_CHERRYUSB_DEVICE=y"
            "CONFIG_CHERRYUSB_DEVICE_MUSB_LISA=y"
            "CONFIG_CHERRYUSB_APP_CLASS=y"
            "CONFIG_ADB=y"
        )
    else()
        list(APPEND BOOT_REQUIRED_CONFIGS
            "CONFIG_TINY_USB=y"
            "CONFIG_TINY_USB_USE_CUSTOM_CONFIG_FILE=y"
            "CONFIG_TINY_USB_CONFIG_FILE=\"boot_tusb_config.h\""
            "CONFIG_TUSB_APP_CLASS=y"
            "CONFIG_ADB=y"
        )
    endif()
endif()

if(DEFINED CONFIG_BOOT_ADB_SYNC)
    if(DEFINED CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        set(_BOOT_REQUIRED_HEAP_SIZE "CONFIG_HEAP_SIZE=0x40000")
    else()
        set(_BOOT_REQUIRED_HEAP_SIZE "CONFIG_HEAP_SIZE=0x30000")
    endif()

    list(APPEND BOOT_REQUIRED_CONFIGS
        "${_BOOT_REQUIRED_HEAP_SIZE}"
        "CONFIG_PSRAM_HEAP_SIZE=0x300000"
        "CONFIG_ADB_MAX_PAYLOAD_SIZE=65536"
        "CONFIG_FREERTOS_FAST_FUNC_SECTION=\".text\""
    )
endif()

if(DEFINED CONFIG_BOOT_CONTROL_STORE_EASYFLASH)
    list(APPEND BOOT_REQUIRED_CONFIGS
        "CONFIG_LS_EF_START_ADDR=${CONFIG_BOOT_CONTROL_STORE_BASE_ADDR}"
        "CONFIG_LS_EF_ENV_AREA_SIZE=${CONFIG_BOOT_CONTROL_STORE_SIZE}"
    )
endif()

list(APPEND BOOT_CONFIGS ${BOOT_REQUIRED_CONFIGS})

set(BOOT_APP_CONFIG_FILE ${CMAKE_BINARY_DIR}/boot/app.config)
foreach(config IN LISTS BOOT_CONFIGS)
    file(APPEND ${BOOT_APP_CONFIG_FILE} "${config}\n")
endforeach()

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
        -DBOARD=${BOARD}
        -DARCS_SDK_BASE=${ARCS_SDK_BASE}
        -DCONFIG_FILES=${BOOT_APP_CONFIG_FILE}
        ${BOOT_DEBUG_CMAKE_ARG}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .
)

# Freeze the app-only payload before merge_boot overwrites the final .bin.
# Use a target instead of an OUTPUT rule so the copy always follows the
# freshly generated headered app bin, even in incremental builds.
add_custom_target(
    boot_app_bin
    COMMAND ${CMAKE_COMMAND} -E copy ${app_bin_path} ${app_without_boot_path}
    DEPENDS ${LISTENAI_BOOT_APP_BIN_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

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

    DEPENDS boot boot_app_bin
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
