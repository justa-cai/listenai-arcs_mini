# PC (sim) NES port: 实现 nes_default.h 声明的平台钩子 (SDL 显示/输入/音频 + 本地文件)
find_package(SDL2 REQUIRED SDL2)

target_sources(nes_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/nes_port_sim.c
)

target_include_directories(nes_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NES_DEMO_DIR}/src/nes/port
    ${NES_DEMO_DIR}/src
)

target_include_directories(nes_core PRIVATE
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(nes_core PUBLIC
    lvgl
    ${SDL2_LIBRARIES}
)
