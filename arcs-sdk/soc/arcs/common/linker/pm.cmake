# PM (light sleep) 段体系总入口：链接片段 + 段重定位规则集中管理。
#
# 这里统一处理两类工作：
#   1. PM 链接片段：pm-snapshot.ld / pm-rom.ld / pm-startup-bss.ld
#      通过 listenai_add_linker_section 注入主链接脚本。pm-snapshot.ld
#      把 PSRAM 尾部的 snapshot 区作为 NOLOAD section 占位，让 linker
#      在业务 .psram.* 长入尾部时直接报 overlap，避免运行时数据损坏。
#   2. PM 段重定位：HAL 库不可改，wake 路径上要求落 SRAM 的段（_PM_RAM_TEXT /
#      _PM_TEXT_TEXT 标注的代码、wifi 关键路径、mem_copy 等）通过 listenai_code_relocate
#      抓到 .fast.text，由 sleep 时 AON 域自动保留。
#
# 入口集中在本文件，便于审计 wake 路径覆盖范围和 PSRAM snapshot 区配置。
# 由 soc/arcs/CMakeLists.txt 通过 include() 引入；调用方需事先 set(_SOC_LD_DIR ...).

# ---------------------------------------------------------------------------
# 链接片段（仅 PM 启用时）
# ---------------------------------------------------------------------------
if(CONFIG_ARCS_HAL_PM)
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/pm-snapshot.ld     SLOT POST_DATA    SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/pm-rom.ld          SLOT ROM          SORT_KEY "10-soc")
    listenai_add_linker_section(FILE ${_SOC_LD_DIR}/pm-startup-bss.ld  SLOT POST_DATA    SORT_KEY "10-soc")
    listenai_add_linker_scatter(SCATZERO .pm_startup_bss)
endif()

# ---------------------------------------------------------------------------
# 段重定位
#   .pm.ramcode    HAL 标 _PM_RAM_TEXT 的函数
#   ._wf_critical  wifi 关键路径函数
#   .text.pm       HAL 标 _PM_TEXT_TEXT 的函数
#   .text.mem_copy HAL wakeup_entry.S 的 mem_copy() 函数体
#
# PM 启用时：四个段都落 SRAM_TEXT，由 sleep 时 AON 域自动保留；ITCM 在 CP
#            软重置后内容丢失，因此不能放 ITCM。
# PM 未启用：保持 master 行为 —— .pm.ramcode 留在 SRAM_TEXT（_PM_RAM_TEXT
#            标注语义），._wf_critical 与 .text.pm 走 ITCM 性能路径，
#            .text.mem_copy 仅 PM 路径需要重定位故不处理。这样非 PM 工程
#            （如 dual_core wifi_ble_netcfg AP 镜像）不被多吃 SRAM 容量。
# ---------------------------------------------------------------------------
if(CONFIG_ARCS_HAL_PM)
    listenai_code_relocate(SECTIONS
        .pm.ramcode
        ._wf_critical
        .text.pm
        .text.mem_copy
        .text.entry
        .pm.ilm
        .text.ap_ilm
        .text.ap_ilm.*
    LOCATION SRAM_TEXT)
else()
    listenai_code_relocate(SECTIONS .pm.ramcode LOCATION SRAM_TEXT)
    listenai_code_relocate(SECTIONS ._wf_critical LOCATION ITCM)
    listenai_code_relocate(SECTIONS .text.pm LOCATION ITCM)
endif()
