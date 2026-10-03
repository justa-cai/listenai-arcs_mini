# game 目录: NES 游戏屏 (PC 与设备共用 view/presenter)
# sim 下同时提供精简的 lisa_ui_app_init(); 设备下产品 UI 由 llm/ 提供。

# ---- NES 核心: 复用 SDK demo 源码, 不复制不改 ----
set(NES_DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../arcs-sdk/demos/arcs/game/nes)
file(GLOB_RECURSE NES_CORE_SOURCES
    ${NES_DEMO_DIR}/src/nes/core/*.c
)

# nes_conf.h 由 demo port 目录提供(内部 include "autoconf.h");
# autoconf.h 只由各平台 port 目录提供, 注意不要在两处放同名头互相遮蔽。
#
# 注意: 核心 (平台无关) 在两个平台都做成独立 nes_core 库 —— set_source_files_properties
# 只对"调用目录创建的 target"生效, 不能对父目录的 lisa_ui_app 设文件属性;
# 仅 nes_port_<plat>.c (需要 SDK 头) 并入 lisa_ui_app。

add_library(nes_core STATIC ${NES_CORE_SOURCES})
set_source_files_properties(${NES_CORE_SOURCES}
    PROPERTIES COMPILE_FLAGS "-O3 -fno-strict-aliasing"
)
target_include_directories(nes_core PUBLIC
    ${NES_DEMO_DIR}/src/nes/inc
)
target_link_libraries(${LISA_UI_LIB_NAME} PUBLIC nes_core)

if(LISA_UI_IS_SIM)
    # PC: lvgl/SDL 由 port/sim 链接进 nes_core
    add_subdirectory(port/sim)
    # sim 专用精简 lisa_ui_app_init (设备版在 llm/lisa_ui_app.c)
    target_sources(${LISA_UI_LIB_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lisa_ui_app_game.c
    )
else()
    # 设备: SRAM 紧张, CHR packed 缓存走 PSRAM 动态分配
    # (nes_rom.c 支持 #ifndef 覆盖容量; 同目录 set_source_files_properties 才生效)
    foreach(_src ${NES_CORE_SOURCES})
        if(_src MATCHES "nes_rom.c$")
            set_source_files_properties(${_src}
                PROPERTIES COMPILE_FLAGS "-O3 -fno-strict-aliasing -DNES_STATIC_CHR_PACKED_ROW_CAPACITY=0")
        endif()
    endforeach()
    add_subdirectory(port/lisa)
endif()

# port 源 (需要 SDK 头) 并入 lisa_ui_app
target_sources(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/nes_game_presenter.c
)

target_include_directories(
    ${LISA_UI_LIB_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../llm
    ${CMAKE_CURRENT_SOURCE_DIR}/../llm/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/roms
)
