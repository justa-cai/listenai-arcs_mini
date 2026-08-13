# SoC-specific linker fragments (outside HAL submodule)
set(_SOC_LD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/linker)
if(EXISTS ${_SOC_LD_DIR})
    # WiFi memory layout (only when WiFi RAM is enabled)
    if(CONFIG_MEM_WFRAM_SIZE GREATER 0)
        listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/wifi-memory.ld SORT_KEY "10-soc")
        listenai_add_linker_section(FILE ${_SOC_LD_DIR}/wifi-noinit.ld   SLOT SECTIONS_START SORT_KEY "10-soc")
    endif()
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/wifi-la-dump.ld  SLOT COMPONENTS SORT_KEY "10-soc")

    # BLE memory layout
    listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/ble-memory.ld SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/ble-aliases.ld   SLOT ALIASES  SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/ble-heap.ld      SLOT SECTIONS_END SORT_KEY "10-soc")

    # IPC shared memory layout
    listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/ipc-memory.ld SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/ipc-aliases.ld   SLOT ALIASES  SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/ipc-shared.ld    SLOT POST_DATA SORT_KEY "10-soc")

    # LUNA shared sections are placed in normal SRAM; no dedicated MEMORY region is needed.
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/luna-sections.ld   SLOT COMPONENTS SORT_KEY "10-soc")
    listenai_add_linker_scatter(SCATLOAD .luna.text)
    listenai_add_linker_scatter(SCATLOAD .luna.data)
    listenai_add_linker_scatter(SCATZERO .luna.bss)

    # Algorithm RAM region
    listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/apram-memory.ld SORT_KEY "10-soc")

    # 内存布局静态校验：SRAM/WFRAM/IPC 不重叠且在合法范围内
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/mem-layout-check.ld SLOT COMPONENTS SORT_KEY "10-soc")

    # PM (light sleep) 段体系：链接片段 + 段重定位规则
    include(${_SOC_LD_DIR}/pm.cmake)

    # Code relocation (非 PM)
    listenai_code_relocate(FILES spiflash.c LOCATION SRAM_RODATA)
    listenai_code_relocate(SECTIONS .text.irq .text.xPortTaskSwitch .text.eclic_mtip_handler LOCATION SRAM_TEXT)

endif()

listenai_compile_definitions(CONFIG_PM_PSRAM=1)

add_subdirectory(hal)

add_subdirectory(common)
