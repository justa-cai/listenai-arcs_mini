set(CORE_FLAGS -mtune=nuclei-300-series -msave-restore)

if (CONFIG_FPU)
    set(FPU_FLAGS -march=rv32imafc_zba_zbb_zbc_zbs -mabi=ilp32f)
else()
    set(FPU_FLAGS -march=rv32imac_zba_zbb_zbc_zbs -mabi=ilp32)
endif()

add_compile_options(${CORE_FLAGS} ${FPU_FLAGS})
add_link_options(${CORE_FLAGS} ${FPU_FLAGS})
