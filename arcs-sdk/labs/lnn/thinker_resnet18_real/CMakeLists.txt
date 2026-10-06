cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

set(THINKER_RESNET18_REAL_CP_TARGET thinker_resnet18_real_cp)
set(THINKER_RESNET18_REAL_AP_TARGET thinker_resnet18_real_ap)

project(${THINKER_RESNET18_REAL_CP_TARGET} C)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/ap_client.c
    src/app.c
    src/board_pins.c
    src/ipc_client.c
    src/ipc_shared.c
    src/main.c
    src/lnn_usb_adb.c
    src/usb_descriptors.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

listenai_include_directories(
    src
)

include(ExternalProject)

ExternalProject_Add(
    remote_core
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/remote
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/remote
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DBOARD=${BOARD}
        $<$<BOOL:${BOARD_SEARCH_PATH}>:-DBOARD_SEARCH_PATH=${BOARD_SEARCH_PATH}>
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

add_dependencies(${PROJECT_NAME} remote_core)

find_program(PYTHON3 python3 REQUIRED)
set(MERGE_SCRIPT "$ENV{ARCS_BASE}/tools/scripts/merge_bins.py")
set(MODEL_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../thinker_resnet18/resources/resnet18_arcs.bin")

add_custom_target(merge_images ALL
    COMMAND ${PYTHON3} ${MERGE_SCRIPT}
            --output ${CMAKE_CURRENT_BINARY_DIR}/merged.bin
            --bin 0x000000 ${CMAKE_CURRENT_BINARY_DIR}/remote/${THINKER_RESNET18_REAL_AP_TARGET}.bin
            --bin 0x200000 ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
            --bin 0x500000 ${MODEL_BIN}
    DEPENDS ${PROJECT_NAME} remote_core
    COMMENT "Merging AP image, CP image and ResNet18 model into merged.bin"
)

message(STATUS "========================================")
message(STATUS "Thinker ResNet18 Real Sample")
message(STATUS "========================================")
message(STATUS "CP Core (camera/LVGL): ${PROJECT_NAME}")
message(STATUS "AP Core (Thinker):     ${THINKER_RESNET18_REAL_AP_TARGET}")
message(STATUS "Model resource:        ${MODEL_BIN}")
message(STATUS "Merged image:          ${CMAKE_CURRENT_BINARY_DIR}/merged.bin")
message(STATUS "========================================")
