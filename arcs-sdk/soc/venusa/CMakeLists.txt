include(hal.cmake)

enable_language(ASM)

# 透传 hartid 给 startup.S, 它会被嵌入 image header reserved[0]。
# CONFIG_HARTID 必须由 Kconfig 体系显式提供 (SoC defconfig 或工程 prj.conf),
# cmake 不提供 fallback, 避免悄无声息地编出错核 image。
if(NOT DEFINED CONFIG_HARTID)
    message(FATAL_ERROR "CONFIG_HARTID is not defined. Set it in SoC defconfig or prj.conf (0=AP, 1=CP).")
endif()
listenai_compile_definitions(-DBOOT_HARTID=${CONFIG_HARTID})

listenai_library_named(startup)

listenai_library_sources(
    startup.S
    soc.c
    freertos_hook.c
    sys_reset_reason.c
    sys_reboot.c
)
listenai_library_sources_ifdef(CONFIG_SOC_EARLY_LOG ${ARCS_SDK_BASE}/soc/common/early_log.c)

listenai_compile_definitions_ifdef(CONFIG_MODULE_FREERTOS CFG_RTOS=1 IC_USE_RTOS=1)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# VenusA SoC 级链接片段：把 libluna.a 的 .sharedmem.* 放入 LUNA 共享 SRAM，
# 并在启动 scatter 阶段完成 ROM->RAM 拷贝和 BSS 清零。
set(_SOC_LD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/linker)
listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/luna-memory.ld SORT_KEY "10-soc")
listenai_add_linker_section(FILE ${_SOC_LD_DIR}/luna-aliases.ld SLOT ALIASES SORT_KEY "10-soc")
listenai_add_linker_section(FILE ${_SOC_LD_DIR}/luna-sections.ld SLOT COMPONENTS SORT_KEY "10-soc")
# 算法 SRAM 只声明 MEMORY 区域供链接器识别；与其它区域的隔离由
# mem-layout-check.ld 的 ASSERT 和各 region LENGTH 保证。
listenai_add_linker_memory(FILE ${_SOC_LD_DIR}/apram-memory.ld SORT_KEY "10-soc")
listenai_add_linker_section(FILE ${_SOC_LD_DIR}/mem-layout-check.ld SLOT COMPONENTS SORT_KEY "10-soc")
if(CONFIG_MEM_LUNA_SHARED_SIZE GREATER 0)
    listenai_add_linker_scatter(SCATLOAD .luna.text)
    listenai_add_linker_scatter(SCATLOAD .luna.data)
    listenai_add_linker_scatter(SCATZERO .luna.bss)
endif()

# 使用由 Kconfig 内存配置驱动的公共 linker script 模板。
listenai_set_linker_script(${ARCS_SDK_BASE}/soc/common/system.ld)

# The VENUSA SPI flash HAL marks flash operation code as .ramcode, but GCC can
# still emit switch jump tables/const data in .rodata. Keep those in SRAM too.
listenai_code_relocate(FILES spiflash.c LOCATION SRAM_RODATA)
