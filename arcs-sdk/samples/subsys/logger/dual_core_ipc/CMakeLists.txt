cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

set(DUAL_CORE_IPC_CP_TARGET dual_core_ipc_cp)
set(DUAL_CORE_IPC_AP_TARGET dual_core_ipc_ap)

project(${DUAL_CORE_IPC_CP_TARGET})

listenai_add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE src/main.c)

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

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf" PRJ_CONF REGEX "^CONFIG_MEM_FLASH_BASE=")
string(REGEX REPLACE "^CONFIG_MEM_FLASH_BASE=(0x[0-9a-fA-F]+)" "\\1" FLASH_BASE_ADDR ${PRJ_CONF})

if(NOT FLASH_BASE_ADDR)
    set(FLASH_OFFSET "0x80000")
else()
    math(EXPR FLASH_OFFSET "${FLASH_BASE_ADDR} - 0x30000000" OUTPUT_FORMAT HEXADECIMAL)
endif()

add_custom_target(merge_images ALL
    COMMAND ${PYTHON3} ${MERGE_SCRIPT}
            --output ${CMAKE_CURRENT_BINARY_DIR}/merged.bin
            --bin 0x00000 ${CMAKE_CURRENT_BINARY_DIR}/remote/${DUAL_CORE_IPC_AP_TARGET}.bin
            --bin ${FLASH_OFFSET} ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    DEPENDS ${PROJECT_NAME} remote_core
    COMMENT "Merging ${DUAL_CORE_IPC_AP_TARGET}.bin and ${PROJECT_NAME}.bin into merged.bin"
)

message(STATUS "========================================")
message(STATUS "Dual-Core IPC Log Sample")
message(STATUS "========================================")
message(STATUS "CP Core (Master): ${PROJECT_NAME}")
message(STATUS "Remote Core (AP): ${DUAL_CORE_IPC_AP_TARGET}")
message(STATUS "========================================")
