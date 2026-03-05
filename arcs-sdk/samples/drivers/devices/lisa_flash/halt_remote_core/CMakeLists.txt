cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

# CP Core (Master) - Main project
project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)

# Build Remote Core (AP) as external project
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

# Add dependency to ensure remote core is built
add_dependencies(${PROJECT_NAME} remote_core)

# Merge binaries
find_program(PYTHON3 python3 REQUIRED)
set(MERGE_SCRIPT "$ENV{ARCS_BASE}/tools/scripts/merge_bins.py")

# Read CONFIG_MEM_FLASH_BASE from prj.conf
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf" PRJ_CONF REGEX "^CONFIG_MEM_FLASH_BASE=")
string(REGEX REPLACE "^CONFIG_MEM_FLASH_BASE=(0x[0-9a-fA-F]+)" "\\1" FLASH_BASE_ADDR ${PRJ_CONF})

if(NOT FLASH_BASE_ADDR)
    message(WARNING "CONFIG_MEM_FLASH_BASE not found in prj.conf, using default offset 0x80000")
    set(FLASH_OFFSET "0x80000")
else()
    # Calculate offset: CONFIG_MEM_FLASH_BASE - 0x30000000
    math(EXPR FLASH_OFFSET "${FLASH_BASE_ADDR} - 0x30000000" OUTPUT_FORMAT HEXADECIMAL)
    message(STATUS "Found FLASH_BASE_ADDR=${FLASH_BASE_ADDR}, calculated offset=${FLASH_OFFSET}")
endif()

add_custom_target(merge_images ALL
    COMMAND ${PYTHON3} ${MERGE_SCRIPT}
            --output ${CMAKE_CURRENT_BINARY_DIR}/merged.bin
            --bin 0x00000 ${CMAKE_CURRENT_BINARY_DIR}/remote/remote.bin
            --bin ${FLASH_OFFSET} ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Merging remote.bin and ${PROJECT_NAME}.bin into merged.bin"
    DEPENDS ${PROJECT_NAME} remote_core
)

# Print build information
message(STATUS "========================================")
message(STATUS "LISA Flash Dual-Core Example")
message(STATUS "========================================")
message(STATUS "CP Core (Master): ${PROJECT_NAME}")
message(STATUS "Remote Core (AP): lisa_flash_dual_core_ap")
message(STATUS "========================================")
