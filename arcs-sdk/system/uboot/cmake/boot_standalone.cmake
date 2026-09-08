set(_BOOT_ARCS_BASE "${ARCS_SDK_BASE}")
if(NOT _BOOT_ARCS_BASE)
    set(_BOOT_ARCS_BASE "$ENV{ARCS_BASE}")
endif()

if(NOT _BOOT_ARCS_BASE)
    message(FATAL_ERROR "ARCS_SDK_BASE or ARCS_BASE must be set")
endif()

get_filename_component(_BOOT_STANDALONE_HELPER_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_BOOT_STANDALONE_DEFAULT_ROOT "${_BOOT_STANDALONE_HELPER_DIR}/.." ABSOLUTE)

if(NOT DEFINED BOOT_STANDALONE_UBOOT_ROOT)
    set(BOOT_STANDALONE_UBOOT_ROOT "${_BOOT_STANDALONE_DEFAULT_ROOT}")
endif()

if(NOT DEFINED BOOT_STANDALONE_PROJECT_NAME)
    set(BOOT_STANDALONE_PROJECT_NAME boot)
endif()

if(NOT DEFINED BOOT_STANDALONE_CONFIG_DEFAULT)
    set(BOOT_STANDALONE_CONFIG_DEFAULT "${BOOT_STANDALONE_UBOOT_ROOT}/prj.conf")
endif()

if(NOT DEFINED CONFIG_DEFAULT)
    set(CONFIG_DEFAULT "${BOOT_STANDALONE_CONFIG_DEFAULT}")
endif()

if(DEFINED BOOT_STANDALONE_CONFIG_FINGERPRINT)
    # The outer boot-only wrapper bumps this value when config content changes.
    set(_BOOT_STANDALONE_CONFIG_FINGERPRINT "${BOOT_STANDALONE_CONFIG_FINGERPRINT}")
endif()

function(_boot_standalone_resolve_config_path out_var config_path)
    if(IS_ABSOLUTE "${config_path}")
        set(${out_var} "${config_path}" PARENT_SCOPE)
    else()
        set(${out_var} "${CMAKE_CURRENT_SOURCE_DIR}/${config_path}" PARENT_SCOPE)
    endif()
endfunction()

function(_boot_standalone_bool_from_configs out_var symbol)
    set(_boot_value OFF)

    foreach(_boot_config IN LISTS ARGN)
        if(NOT EXISTS "${_boot_config}")
            continue()
        endif()

        file(
            STRINGS "${_boot_config}" _boot_matches
            REGEX "^(CONFIG_${symbol}=y|# CONFIG_${symbol} is not set)$"
        )

        foreach(_boot_line IN LISTS _boot_matches)
            if(_boot_line STREQUAL "CONFIG_${symbol}=y")
                set(_boot_value ON)
            else()
                set(_boot_value OFF)
            endif()
        endforeach()
    endforeach()

    set(${out_var} "${_boot_value}" PARENT_SCOPE)
endfunction()

function(_boot_standalone_config_has_assignment_in_configs out_var symbol)
    set(_boot_found OFF)

    foreach(_boot_config IN LISTS ARGN)
        if(NOT EXISTS "${_boot_config}")
            continue()
        endif()

        file(
            STRINGS "${_boot_config}" _boot_matches
            REGEX "^(CONFIG_${symbol}=|# CONFIG_${symbol} is not set)$"
        )

        if(_boot_matches)
            set(_boot_found ON)
            break()
        endif()
    endforeach()

    set(${out_var} "${_boot_found}" PARENT_SCOPE)
endfunction()

function(_boot_standalone_config_has_assignment out_var symbol config_path)
    set(_boot_found OFF)

    if(EXISTS "${config_path}")
        file(STRINGS "${config_path}" _boot_matches REGEX "^CONFIG_${symbol}=")
        if(_boot_matches)
            set(_boot_found ON)
        endif()
    endif()

    set(${out_var} "${_boot_found}" PARENT_SCOPE)
endfunction()

function(_boot_standalone_relocate_files location)
    foreach(_boot_file ${ARGN})
        listenai_code_relocate(FILES ${_boot_file} LOCATION ${location})
    endforeach()
endfunction()

_boot_standalone_resolve_config_path(_boot_config_default_path "${CONFIG_DEFAULT}")
set(_BOOT_STANDALONE_CONFIG_SCAN_FILES "${_boot_config_default_path}")
if(DEFINED CONFIG_FILES)
    foreach(_boot_config_file IN LISTS CONFIG_FILES)
        _boot_standalone_resolve_config_path(_boot_config_file_path "${_boot_config_file}")
        list(APPEND _BOOT_STANDALONE_CONFIG_SCAN_FILES "${_boot_config_file_path}")
    endforeach()
endif()

_boot_standalone_bool_from_configs(
    _BOOT_STANDALONE_ENABLE_BOOT_SECOND_STAGE
    BOOT_SECOND_STAGE
    ${_BOOT_STANDALONE_CONFIG_SCAN_FILES}
)
_boot_standalone_bool_from_configs(
    _BOOT_STANDALONE_ENABLE_BOOT_ADB
    BOOT_ADB
    ${_BOOT_STANDALONE_CONFIG_SCAN_FILES}
)
_boot_standalone_bool_from_configs(
    _BOOT_STANDALONE_ENABLE_BOOT_ADB_SYNC
    BOOT_ADB_SYNC
    ${_BOOT_STANDALONE_CONFIG_SCAN_FILES}
)
_boot_standalone_bool_from_configs(
    _BOOT_STANDALONE_ENABLE_BOOT_ADB_BACKEND_CHERRYUSB
    BOOT_ADB_BACKEND_CHERRYUSB
    ${_BOOT_STANDALONE_CONFIG_SCAN_FILES}
)

set(_BOOT_STANDALONE_REQUIRED_CONFIGS "")
list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
    "# CONFIG_LINK_OPTION_LISTENAI_LIBRARY_WHOLE_ARCHIVE is not set"
    "CONFIG_LINK_OPTION_LISTENAI_LIBRARY_GROUP=y"
)

if(_BOOT_STANDALONE_ENABLE_BOOT_ADB OR _BOOT_STANDALONE_ENABLE_BOOT_ADB_SYNC)
    if(_BOOT_STANDALONE_ENABLE_BOOT_ADB_BACKEND_CHERRYUSB)
        list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
            "CONFIG_CHERRYUSB=y"
            "CONFIG_CHERRYUSB_DEVICE=y"
            "CONFIG_CHERRYUSB_DEVICE_MUSB_LISA=y"
            "CONFIG_CHERRYUSB_APP_CLASS=y"
            "CONFIG_ADB=y"
        )
    else()
        list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
            "CONFIG_TINY_USB=y"
            "CONFIG_TINY_USB_USE_CUSTOM_CONFIG_FILE=y"
            "CONFIG_TINY_USB_CONFIG_FILE=\"boot_tusb_config.h\""
            "CONFIG_TUSB_APP_CLASS=y"
            "CONFIG_ADB=y"
        )
    endif()
endif()

if(_BOOT_STANDALONE_ENABLE_BOOT_ADB OR _BOOT_STANDALONE_ENABLE_BOOT_ADB_SYNC)
    if(_BOOT_STANDALONE_ENABLE_BOOT_ADB_BACKEND_CHERRYUSB)
        set(_BOOT_STANDALONE_BOOT_ADB_HEAP_SIZE "0x42000")
    else()
        set(_BOOT_STANDALONE_BOOT_ADB_HEAP_SIZE "0x30000")
    endif()
    list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
        "CONFIG_HEAP_SIZE=${_BOOT_STANDALONE_BOOT_ADB_HEAP_SIZE}"
    )
endif()

if(_BOOT_STANDALONE_ENABLE_BOOT_ADB_SYNC)
    if(_BOOT_STANDALONE_ENABLE_BOOT_ADB_BACKEND_CHERRYUSB)
        set(_BOOT_STANDALONE_BOOT_ADB_PAYLOAD_SIZE "51200")
    else()
        set(_BOOT_STANDALONE_BOOT_ADB_PAYLOAD_SIZE "65536")
    endif()
    list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
        "CONFIG_PSRAM_HEAP_SIZE=0x300000"
        "CONFIG_ADB_MAX_PAYLOAD_SIZE=${_BOOT_STANDALONE_BOOT_ADB_PAYLOAD_SIZE}"
        "CONFIG_FREERTOS_FAST_FUNC_SECTION=\".fast_text\""
    )
endif()

foreach(_boot_default_disabled_symbol CONSOLE LOG)
    _boot_standalone_config_has_assignment_in_configs(
        _boot_has_explicit_setting
        ${_boot_default_disabled_symbol}
        ${_BOOT_STANDALONE_CONFIG_SCAN_FILES}
    )

    if(NOT _boot_has_explicit_setting)
        list(APPEND _BOOT_STANDALONE_REQUIRED_CONFIGS
            "# CONFIG_${_boot_default_disabled_symbol} is not set"
        )
    endif()
endforeach()

if(_BOOT_STANDALONE_REQUIRED_CONFIGS)
    set(_BOOT_STANDALONE_REQUIRED_CONFIG_FILE
        "${CMAKE_BINARY_DIR}/misc/generated/boot_standalone_required.conf"
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/misc/generated")
    file(WRITE "${_BOOT_STANDALONE_REQUIRED_CONFIG_FILE}" "")

    foreach(_boot_required_config IN LISTS _BOOT_STANDALONE_REQUIRED_CONFIGS)
        file(APPEND "${_BOOT_STANDALONE_REQUIRED_CONFIG_FILE}" "${_boot_required_config}\n")
    endforeach()

    list(APPEND CONFIG_FILES "${_BOOT_STANDALONE_REQUIRED_CONFIG_FILE}")
endif()

set(LISTENAI_MODULES_DIR_LIST
    ${_BOOT_ARCS_BASE}/soc/arcs/hal
    ${_BOOT_ARCS_BASE}/modules/freertos
    ${_BOOT_ARCS_BASE}/modules/heap
    ${_BOOT_ARCS_BASE}/modules/chryusb
    ${_BOOT_ARCS_BASE}/modules/tinyusb
    ${_BOOT_ARCS_BASE}/modules/easylogger
    ${_BOOT_ARCS_BASE}/modules/letter-shell
    ${_BOOT_ARCS_BASE}/modules/fs
    ${_BOOT_ARCS_BASE}/boards
    ${_BOOT_ARCS_BASE}/drivers
    ${_BOOT_ARCS_BASE}/components
    ${BOOT_STANDALONE_UBOOT_ROOT}/src
)

find_package(listenai-cmake REQUIRED HINTS ${BOOT_STANDALONE_UBOOT_ROOT} ${_BOOT_ARCS_BASE})

project(${BOOT_STANDALONE_PROJECT_NAME} C CXX ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT DEFINED BOARD)
    set(BOARD arcs_evb CACHE STRING "Target board")
endif()

message(STATUS "boot target board: ${BOARD}")

listenai_include_directories(
    ${_BOOT_ARCS_BASE}/boards/${BOARD}
    ${_BOOT_ARCS_BASE}/soc/common/include
)

set(BOOT_STANDALONE_BUILD ON)

set(CONFIG_SOC_ARCS ON)
set(CONFIG_ARCS_AP_CORE ON)
set(CONFIG_HARTID 0)
set(CONFIG_FILE_SYSTEM ON)
set(CONFIG_DISK_DRIVER ON)
set(CONFIG_SDK_MODULE_LETTER_SHELL ON)
set(CONFIG_SDK_MODULE_EASYLOGGER ON)

listenai_add_executable(${PROJECT_NAME})
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)

if(DEFINED CONFIG_BOOT_ADB)
    # Recovery ADB needs TinyUSB state in SRAM, so use a neutral archive name.
    if(TARGET psram_tinyusb)
        set_target_properties(psram_tinyusb PROPERTIES OUTPUT_NAME boot_tinyusb)
    endif()
endif()

if(DEFINED CONFIG_LISA_SDMMC_DEVICE)
    # Standalone boot builds do not enable global whole-archive, so keep SDMMC.
    target_link_libraries(${PROJECT_NAME} PRIVATE
        -Wl,--whole-archive
        lisa_sdmmc_device
        -Wl,--no-whole-archive
    )
endif()

if(DEFINED CONFIG_BOOT_DISPLAY)
    # LISA_DEVICE_REGISTER 项只通过 .lisa_device_registry.* 段被引用，--gc-sections
    # 会把这些驱动 object 丢掉，所以这里强制 whole-archive 把注册项和 pinmux 弱符号留下。
    set(_boot_display_libs lisa_device lisa_display_device)
    if(DEFINED CONFIG_LISA_SPI_DEVICE)
        list(APPEND _boot_display_libs lisa_spi)
    endif()
    if(DEFINED CONFIG_LISA_GPIO_DEVICE)
        list(APPEND _boot_display_libs lisa_gpio_device)
    endif()
    if(DEFINED CONFIG_LISA_PWM)
        list(APPEND _boot_display_libs lisa_pwm)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        -Wl,--whole-archive
        ${_boot_display_libs}
        module_boards
        -Wl,--no-whole-archive
    )

    # display 驱动 + bus/panel/pinmux 塞不下 SRAM，整体搬到 PSRAM。
    foreach(_lib IN LISTS _boot_display_libs)
        listenai_code_relocate(LIBRARY ${_lib} LOCATION PSRAM)
    endforeach()
endif()

listenai_add_linker_section(
    FILE ${BOOT_STANDALONE_UBOOT_ROOT}/linker/boot-components.ld
    SLOT COMPONENTS
    SORT_KEY "05-boot"
)

set(BOOT_STANDALONE_FLASH_BASE 0x30000000)
set(BOOT_STANDALONE_FLASH_SIZE 0x3F000)
set(BOOT_STANDALONE_CONFIG_BASE 0x3003d000)

listenai_compile_definitions(
    CONFIG_BOARD_NAME="${BOARD}"
    CONFIG_SOC_ARCS=1
    CONFIG_ARCS_AP_CORE=1
    CONFIG_HARTID=0
    CONFIG_MEM_SRAM_BASE=0x20050000
    CONFIG_MEM_SRAM_SIZE=0x60000
    CONFIG_MEM_FLASH_BASE=${BOOT_STANDALONE_FLASH_BASE}
    CONFIG_MEM_FLASH_SIZE=${BOOT_STANDALONE_FLASH_SIZE}
    CONFIG_MEM_ILM_BASE=0x00080000
    CONFIG_MEM_ILM_SIZE=0x4000
    CONFIG_MEM_DLM_BASE=0x00100000
    CONFIG_MEM_DLM_SIZE=0x2000
    CONFIG_MEM_INTERRUPT_STACK_SIZE=0x1800
    CONFIG_MEM_HEAP_SIZE=512
    CONFIG_LISA_FLASH_ARCS_CONTROLLER_BASE_ADDR=0x47600000
    CONFIG_LISA_FLASH_ARCS_DATA_WIDTH=4
    CONFIG_LISA_FLASH_ARCS_SCLK_DIV=255
    CONFIG_LISA_FLASH_ARCS_ADDR_BYTES=3
    CONFIG_LISA_FLASH_ARCS_WRITE_PROTECT_ENABLE=1
    CONFIG_LISA_FLASH_ARCS_DISABLE_INTERRUPTS=1
    CONFIG_LISA_FLASH_ARCS_ERASE_SECTOR_SIZE=4096
    CONFIG_LISA_FLASH_ARCS_WRITE_BLOCK_SIZE=4
    CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_ADDR=0x30000000
    CONFIG_LISA_FLASH_ARCS_FLASH_PHYS_RESERVE_SIZE=0x10000000
)

listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_LIST_DIR}/boot_ilm_noinit.ld
    SLOT COMPONENTS
    SORT_KEY "10-boot"
)

set(_BOOT_STANDALONE_FALLBACK_DEFINITIONS "")
set(_BOOT_STANDALONE_GENERATED_CONFIG "${CMAKE_BINARY_DIR}/.config")

_boot_standalone_config_has_assignment(
    _BOOT_STANDALONE_HAS_LSFS_MAX_NUMBER
    LSFS_MAX_NUMBER
    "${_BOOT_STANDALONE_GENERATED_CONFIG}"
)
if(NOT _BOOT_STANDALONE_HAS_LSFS_MAX_NUMBER)
    list(APPEND _BOOT_STANDALONE_FALLBACK_DEFINITIONS CONFIG_LSFS_MAX_NUMBER=8)
endif()

_boot_standalone_config_has_assignment(
    _BOOT_STANDALONE_HAS_EASYLOGGER_LINE_BUF_SIZE
    EASYLOGGER_LINE_BUF_SIZE
    "${_BOOT_STANDALONE_GENERATED_CONFIG}"
)
if(NOT _BOOT_STANDALONE_HAS_EASYLOGGER_LINE_BUF_SIZE)
    list(APPEND _BOOT_STANDALONE_FALLBACK_DEFINITIONS CONFIG_EASYLOGGER_LINE_BUF_SIZE=256)
endif()

if(_BOOT_STANDALONE_FALLBACK_DEFINITIONS)
    listenai_compile_definitions(${_BOOT_STANDALONE_FALLBACK_DEFINITIONS})
endif()

if(_BOOT_STANDALONE_ENABLE_BOOT_ADB_SYNC AND _BOOT_STANDALONE_ENABLE_BOOT_ADB_BACKEND_CHERRYUSB)
    # Match the fast boot reference by keeping the USB RX/TX hot path out of XIP.
    _boot_standalone_relocate_files(ITCM
        adb_device.c
        usb_dc_musb.c
        usbd_core.c
    )
    _boot_standalone_relocate_files(DTCM
        usb_dc_musb.c
        usbd_core.c
    )
    _boot_standalone_relocate_files(SRAM_TEXT
        adb.c
        adb_services.c
        adb_sync.c
        adb_sync_ext_disk.c
        adb_sync_stage.c
        disk_access.c
        fs_env.c
        lisa_sdmmc_arcs.c
        lsfs.c
        lsfs_fat.c
        sdmmc.c
        usb_glue_lisa.c
        usb_osal_freertos.c
    )
    _boot_standalone_relocate_files(SRAM_RODATA
        adb_device.c
        adb_sync.c
        lisa_sdmmc_arcs.c
        lsfs.c
        lsfs_fat.c
        usb_dc_musb.c
        usbd_core.c
    )
endif()

listenai_set_linker_script(${_BOOT_ARCS_BASE}/soc/common/system.ld)

set(BOOT_STANDALONE_TARGET "${PROJECT_NAME}")
set(BOOT_STANDALONE_BIN_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")

if(_BOOT_STANDALONE_ENABLE_BOOT_SECOND_STAGE)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE}
                ${BOOT_STANDALONE_UBOOT_ROOT}/test/check_stage2_no_flash_xip.py
                --relf ${PROJECT_NAME}.relf
                --symb ${PROJECT_NAME}.symb
                --lst ${PROJECT_NAME}.lst
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
