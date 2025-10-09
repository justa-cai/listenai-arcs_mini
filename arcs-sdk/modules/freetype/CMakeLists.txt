if (DEFINED CONFIG_SDK_MODULE_FREETYPE)

    # 禁用 HarfBuzz 和 Brotli 依赖，因为它们与 RISC-V 目标不兼容
    # 必须在 include 原始 CMakeLists.txt 之前设置这些变量
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "Disable use of system harfbuzz" FORCE)
    set(FT_DISABLE_BROTLI TRUE CACHE BOOL "Disable use of system brotli" FORCE)
    
    # Include the original CMakeLists.txt
    include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt.orig)

    listenai_link_libraries(freetype)

endif()
