# 设备 (lisa/arcs-mini) NES port: port 源并入 lisa_ui_app (需要 SDK 头)
target_sources(${LISA_UI_LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/nes_port_lisa.c
)

target_include_directories(${LISA_UI_LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NES_DEMO_DIR}/src/nes/port
    ${NES_DEMO_DIR}/src
)

# nes_core 独立库 (在父 game/CMakeLists.txt 创建, 同目录作用域) 的头路径
target_include_directories(nes_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NES_DEMO_DIR}/src/nes/port
)
