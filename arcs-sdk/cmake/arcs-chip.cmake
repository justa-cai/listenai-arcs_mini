# -mtune=nuclei-300-series 需要 Nuclei 专属 gcc。本机 listenai-dev-tools/gcc
# （T-Head 14.2.1）不识别该调优名——此时设置环境变量 ARCS_MTUNE_NUCLEI_OFF=1
# 降级为仅 -msave-restore（纯调度调优，语义可省，不影响 ABI/正确性）。
if(DEFINED ENV{ARCS_MTUNE_NUCLEI_OFF})
    set(CORE_FLAGS -msave-restore)
else()
    set(CORE_FLAGS -mtune=nuclei-300-series -msave-restore)
endif()

if (CONFIG_FPU)
    set(FPU_FLAGS -march=rv32imafc_zba_zbb_zbc_zbs -mabi=ilp32f)
else()
    set(FPU_FLAGS -march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32)
endif()

add_compile_options(${CORE_FLAGS} ${FPU_FLAGS})
add_link_options(${CORE_FLAGS} ${FPU_FLAGS})
