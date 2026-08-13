cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(thinker_resnet18_real_ap C)

set(CONFIG_ALGO_DMA_COPY_USE_DMA 1)
set(CONFIG_ALGO_DMA_CHANNEL_PERSISTENT 1)

add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/../../common/arcs
    ${CMAKE_CURRENT_BINARY_DIR}/lnn_common_arcs
)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    ../src/ipc_shared.c
    src/main.c
    src/resnet18_real_ap.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ../include
)

listenai_add_linker_section(
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/../linker/thinker-apram.ld
    SLOT COMPONENTS
    SORT_KEY "50-app"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    thinker
    arcs_dma_cpy
)
