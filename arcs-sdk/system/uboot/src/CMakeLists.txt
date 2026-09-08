enable_language(ASM)

listenai_library_named(boot_app)

listenai_library_compile_options(
    PRIVATE
    -Oz
    -fomit-frame-pointer
    -foptimize-sibling-calls
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

set(BOOT_HAL_BASE ${ARCS_SDK_BASE}/soc/arcs/hal)
set(BOOT_HAL_CHIP_BASE ${BOOT_HAL_BASE}/chip/arcs)

listenai_compile_definitions(
    CHIP=4
    IC_BOARD=1
    PSRAM_LOG_CHECK=0
    IC_HAL_DCACHE_SIZE=32
    CONFIG_EXT_RAM
    IC_HAL_DCACHE_IS_COHERENT=0
    BOOT_HARTID=0
    MBX_AP_WORK=1
    CFG_RTOS=1
    IC_USE_RTOS=1
    BOARD_BOOTCLOCKRUN_SPI0_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_UART0_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_SPI1_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_UART1_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_SPI2_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_UART2_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_GPT_T0_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_GPT_S_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_GPADC_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_IR_TX_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_RGB_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_SDIO_H_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_QSPI0_CLK_DEF=0
    BOARD_BOOTCLOCKRUN_QSPI1_CLK_DEF=0
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/xzdec/include
    ${BOOT_HAL_CHIP_BASE}/driver/debug
    ${ARCS_SDK_BASE}/system
    ${ARCS_SDK_BASE}/system/init
    ${ARCS_SDK_BASE}/system/log
    ${ARCS_SDK_BASE}/system/os/inc
    ${ARCS_SDK_BASE}/modules/letter-shell/src
)

if(CONFIG_BOOT_SECOND_STAGE)
    listenai_library_sources(
        start.S
        boot.c
        scatload.c
        syslog.c
        stdio_shim.c
    )
else()
    listenai_library_sources(
        start.S
        boot.c
        scatload.c
        cache.c
        runtime_globals.c
    )
endif()

set(BOOT_PRE_PSRAM_SOURCES
    boot.c
    scatload.c
    system.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_FACTORY_PARAMS
    boot_factory_params.c
)

if(CONFIG_BOOT_FACTORY_PARAMS)
    list(APPEND BOOT_PRE_PSRAM_SOURCES
        boot_factory_params.c
    )
endif()

listenai_library_sources_ifdef(CONFIG_BOOT_CHECK_POWER_KEY
    boot_gpio_check.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_POWER_GUARD
    boot_power_guard.c
)

if(CONFIG_BOOT_CHECK_POWER_KEY)
    list(APPEND BOOT_PRE_PSRAM_SOURCES
        boot_gpio_check.c
    )
endif()

if(CONFIG_BOOT_POWER_GUARD)
    list(APPEND BOOT_PRE_PSRAM_SOURCES
        boot_power_guard.c
    )
endif()

set(BOOT_RUNTIME_SRAM_SOURCES
    main.c
    syslog.c
    boot_watchdog.c
    IOMuxManager.c
    ${ARCS_SDK_BASE}/system/heap/sysheap.c
)

set(BOOT_EARLY_SRAM_SOURCES
    ${BOOT_PRE_PSRAM_SOURCES}
    ${BOOT_RUNTIME_SRAM_SOURCES}
)

# These sources run before the second-stage PSRAM runtime is fully safe to
# execute, so keep their prologues self-contained instead of calling relocated
# libgcc helpers.
set_source_files_properties(${BOOT_EARLY_SRAM_SOURCES}
    PROPERTIES
    COMPILE_OPTIONS "-mno-save-restore"
)

# Keep boot_f/boot_s loader helpers and early-init code out of this list. They
# must remain runnable before the PSRAM shadow image is loaded.
set(BOOT_RUNTIME_PSRAM_SOURCES
    stdio_shim.c
    boot_flash.c
    boot_config.c
    boot_ota_handoff.c
    boot_recovery_adb_gate.c
    backtrace.c
    cache_ap.c
    freertos_hook.c
    trng.c
    lisa_mem.c
    lisa_mutex.c
    lisa_time.c
    boot_adb_runtime.c
    boot_adb_backend_tinyusb.c
    boot_adb_usb_descriptors.c
    boot_tusb_appclass_link.c
    boot_adb_backend_cherryusb.c
    boot_adb_cherryusb_descriptors.c
    boot_adb_storage.c
    boot_lisa_log_backend.c
    boot_recovery_shell_cmds.c
    boot_sys_time.c
    lisa_log.c
    lisa_log_frontend_easylog.c
    boot_adb_disk_policy.c
    boot_md5.c
    boot_nvs.c
    boot_partab.c
    boot_env.c
    boot_ota_manifest.c
    boot_ota_source.c
    boot_ota_lifecycle.c
    boot_ota_request.c
    boot_remap.c
    boot_crc32.c
    boot_ota.c
    boot_control_store_flash.c
    boot_control_store_none.c
    boot_control_store_test.c
    cJSON.c
    xz_crc32.c
    xz_dec_stream.c
    xz_dec_lzma2.c
    xz_dec_bcj.c
)

if(CONFIG_BOOT_DISPLAY)
    list(APPEND BOOT_RUNTIME_PSRAM_SOURCES
        boot_display.c
        ${ARCS_SDK_BASE}/system/os/lisa_semaphore.c
        ${ARCS_SDK_BASE}/system/os/lisa_thread.c
    )
endif()

if(CONFIG_BOOT_FEATURES)
    list(APPEND BOOT_RUNTIME_PSRAM_SOURCES
        boot_features.c
    )
endif()

# Early PSRAM runtime still hits unstable entry stubs on real hardware, so keep
# these prologues self-contained just like the SRAM bring-up path.
set_source_files_properties(${BOOT_RUNTIME_PSRAM_SOURCES}
    PROPERTIES
    COMPILE_OPTIONS "-mno-save-restore"
)

set_source_files_properties(
    ${ARCS_SDK_BASE}/system/log/lisa_log.c
    ${ARCS_SDK_BASE}/system/log/lisa_log_frontend_easylog.c
    PROPERTIES
    COMPILE_FLAGS "-mno-save-restore"
)

if(CONFIG_BOOT_SECOND_STAGE)
    # SystemInit still runs before the second-stage PSRAM runtime is fully ready,
    # and it touches global state before PMP grants PSRAM access, so keep the
    # whole translation unit in SRAM.
    listenai_code_relocate(FILES system.c LOCATION SRAM)

    # Keep the first runtime bring-up translation units fully in SRAM until the
    # PSRAM path proves stable on real hardware.
    listenai_code_relocate(FILES ${BOOT_RUNTIME_SRAM_SOURCES} LOCATION SRAM)

    listenai_code_relocate(FILES ${BOOT_RUNTIME_PSRAM_SOURCES} LOCATION PSRAM)
endif()

listenai_library_sources_ifdef(CONFIG_BOOT_APP_CORE_AUTO
    boot_app_target.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_FEATURES
    boot_features.c
)
listenai_library_sources_ifdef(CONFIG_BOOT_SECOND_STAGE
    main.c
    boot_default_app.c
    boot_flash.c
    boot_config.c
    boot_reset_cause.c
    boot_ota_handoff.c
    boot_stage_gate.c
    boot_recovery_adb_gate.c
    boot_watchdog.c
    backtrace.c
    cache_ap.c
    system.c
    freertos_hook.c
    trng.c
    ${ARCS_SDK_BASE}/system/heap/sysheap.c
    ${ARCS_SDK_BASE}/system/os/lisa_mem.c
    ${ARCS_SDK_BASE}/system/os/lisa_mutex.c
    ${ARCS_SDK_BASE}/system/os/lisa_time.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_DISPLAY
    boot_display.c
    ${ARCS_SDK_BASE}/system/os/lisa_semaphore.c
    ${ARCS_SDK_BASE}/system/os/lisa_thread.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_ADB
    boot_adb_runtime.c
)

if(CONFIG_BOOT_ADB_BACKEND_TINYUSB)
    listenai_library_sources(
        boot_adb_backend_tinyusb.c
        boot_adb_usb_descriptors.c
        boot_tusb_appclass_link.c
    )
elseif(CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
    listenai_library_sources(
        boot_adb_backend_cherryusb.c
        boot_adb_cherryusb_descriptors.c
    )
endif()

listenai_library_sources_ifdef(CONFIG_BOOT_ADB_SYNC
    boot_adb_storage.c
)

listenai_library_sources_ifdef(CONFIG_BOOT_ADB_SHELL
    ${ARCS_SDK_BASE}/system/log/lisa_log.c
    boot_lisa_log_backend.c
    boot_recovery_shell_cmds.c
    boot_sys_time.c
)

if(DEFINED CONFIG_BOOT_ADB_SHELL AND DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    listenai_library_sources(
        ${ARCS_SDK_BASE}/system/log/lisa_log_frontend_easylog.c
    )
endif()

if(DEFINED CONFIG_BOOT_ADB_SHELL OR DEFINED CONFIG_BOOT_OTA_PACKAGE)
    listenai_library_sources(
        boot_md5.c
    )
endif()

listenai_library_sources_ifdef(CONFIG_BOOT_ADB_SYNC
    boot_adb_disk_policy.c
)

set(BOOT_USES_FILE_SYNC_STORAGE OFF)
if(CONFIG_BOOT_ADB_SYNC OR CONFIG_BOOT_OTA_SOURCE_TF)
    set(BOOT_USES_FILE_SYNC_STORAGE ON)
endif()


listenai_library_sources_ifdef(CONFIG_BOOT_OTA_PACKAGE
    boot_nvs.c
    boot_partab.c
    boot_env.c
    boot_ota_manifest.c
    boot_ota_source.c
    boot_ota_lifecycle.c
    boot_ota_request.c
    boot_remap.c
    boot_crc32.c
    boot_ota.c
    ${ARCS_SDK_BASE}/modules/cjson/cJSON.c
    xzdec/xz_crc32.c
    xzdec/xz_dec_stream.c
    xzdec/xz_dec_lzma2.c
    xzdec/xz_dec_bcj.c
)

if(DEFINED CONFIG_BOOT_OTA_PACKAGE AND DEFINED CONFIG_BOOT_CONTROL_STORE_FLASH)
    listenai_library_sources(
        boot_control_store_flash.c
    )
endif()

if(DEFINED CONFIG_BOOT_OTA_PACKAGE AND DEFINED CONFIG_BOOT_CONTROL_STORE_NONE)
    listenai_library_sources(
        boot_control_store_none.c
    )
endif()

if(DEFINED CONFIG_BOOT_OTA_PACKAGE AND DEFINED CONFIG_BOOT_CONTROL_STORE_TEST)
    listenai_library_sources(
        boot_control_store_test.c
    )
endif()

if(TARGET hal)
    set(BOOT_HAL_LIBRARY hal)
else()
    set(BOOT_HAL_LIBRARY arcs-hal)
endif()

target_link_libraries(boot_app PUBLIC ${BOOT_HAL_LIBRARY} freertos heap)

if(BOOT_USES_FILE_SYNC_STORAGE)
    target_link_libraries(boot_app PUBLIC lsfs)
endif()

if(CONFIG_BOOT_ADB_SYNC OR CONFIG_BOOT_OTA_SOURCE_TF)
    target_link_libraries(boot_app PUBLIC disk)
endif()

if(DEFINED CONFIG_BOOT_ADB_SHELL)
    target_link_libraries(boot_app PUBLIC letter-shell)
endif()

if(DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
    target_link_libraries(boot_app PUBLIC easylogger)
    if(CONFIG_BOOT_SECOND_STAGE AND DEFINED CONFIG_BOOT_ADB_SHELL)
        target_link_libraries(boot_app PUBLIC "-u elog_output")
    endif()
endif()

if(DEFINED CONFIG_BOOT_OTA_PACKAGE)
    listenai_include_directories(
        ${ARCS_SDK_BASE}/modules/cjson
    )
    target_link_libraries(boot_app PUBLIC m)
endif()

if(CONFIG_BOOT_SECOND_STAGE)
    listenai_code_relocate(LIBRARY ${BOOT_HAL_LIBRARY} LOCATION PSRAM)
    listenai_code_relocate(LIBRARY c LOCATION PSRAM EXCLUDE_OBJECTS libc_a-impure.o libc_a-rand.o)
    listenai_code_relocate(LIBRARY c_nano LOCATION PSRAM EXCLUDE_OBJECTS libc_a-impure.o libc_a-rand.o)
    listenai_code_relocate(LIBRARY gcc LOCATION SRAM)
    listenai_code_relocate(LIBRARY freertos LOCATION PSRAM)
    listenai_code_relocate(FILES ${ARCS_SDK_BASE}/modules/freertos/portable/MemMang/heap_3.c LOCATION SRAM)
    listenai_code_relocate(LIBRARY heap LOCATION SRAM)
    listenai_code_relocate(LIBRARY module_boards LOCATION PSRAM)
    listenai_code_relocate(LIBRARY lisa_device LOCATION PSRAM)

    if(DEFINED CONFIG_BOOT_ADB)
        listenai_code_relocate(LIBRARY adb LOCATION PSRAM)
    endif()

    if(DEFINED CONFIG_BOOT_ADB_BACKEND_TINYUSB)
        # Standalone boot renames psram_tinyusb to boot_tinyusb, so relocate
        # the final archive name that actually appears on the boot_s link line.
        listenai_code_relocate(LIBRARY boot_tinyusb LOCATION PSRAM)
        listenai_code_relocate(LIBRARY tinyusb-appclass LOCATION PSRAM)
    endif()

    if(DEFINED CONFIG_BOOT_ADB_BACKEND_CHERRYUSB)
        listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)
    endif()

    if(DEFINED CONFIG_LISA_SDMMC_DEVICE)
        listenai_code_relocate(LIBRARY lisa_sdmmc_device LOCATION PSRAM)
    endif()

    if(DEFINED CONFIG_SDK_MODULE_EASYLOGGER)
        listenai_code_relocate(LIBRARY easylogger LOCATION PSRAM)
    endif()

    if(DEFINED CONFIG_BOOT_ADB_SHELL)
        listenai_code_relocate(LIBRARY letter-shell LOCATION PSRAM)
    endif()

    if(BOOT_USES_FILE_SYNC_STORAGE)
        listenai_code_relocate(LIBRARY elmfat LOCATION PSRAM)
        listenai_code_relocate(LIBRARY lsfs LOCATION PSRAM)
        listenai_code_relocate(LIBRARY disk LOCATION PSRAM)
        listenai_code_relocate(LIBRARY fs_env LOCATION PSRAM)
    endif()
endif()
