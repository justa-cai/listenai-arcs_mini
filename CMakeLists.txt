cmake_minimum_required(VERSION 3.13)
include(version.cmake)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
    $ENV{ARCS_BASE}/../modules
    $ENV{ARCS_BASE}/../components/lisa_kv
    $ENV{ARCS_BASE}/../components/ipclsf
    $ENV{ARCS_BASE}/../components/lisa_porting
    $ENV{ARCS_BASE}/../components/display
    $ENV{ARCS_BASE}/../components/touch
    $ENV{ARCS_BASE}/../components/tinyusb-appclass
    $ENV{ARCS_BASE}/../components/core_spinlock
    $ENV{ARCS_BASE}/../components/irq_proxy
    $ENV{ARCS_BASE}/../components/camera
    $ENV{ARCS_BASE}/../components/flash
    # ${CMAKE_CURRENT_SOURCE_DIR}/lisa_player/
    # ${CMAKE_CURRENT_SOURCE_DIR}/third-party/aacdec
    # ${CMAKE_CURRENT_SOURCE_DIR}/third-party/mp3dec
    # ${CMAKE_CURRENT_SOURCE_DIR}/third-party/coreHTTP
    # ${CMAKE_CURRENT_SOURCE_DIR}/third-party/mempool
    # ${CMAKE_CURRENT_SOURCE_DIR}/third-party/speexdsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Werror)
find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(aiui)

listenai_include_directories(
    ./
    ./src
    ./src/audio
    ./src/utils
    ./src/wifi
	./src/middleware
	# ./src/net
	./src/system
	./src/libc
	./src/tone
	./src/flash
	./src/player
	./src/cloud
	./src/cloud/config
	./src/cloud/core
	./src/cloud/proc
	./src/cloud/recognizer
    ./src/cloud/stream_text
    ./src/cloud/mcp
    ./src/cloud/mcp/tools
)

add_definitions(-DLISTEN_CLOUD -DCONFIG_EXT_RAM)
aux_source_directory(./src/audio AUDIO_SRC_FILES)
# aux_source_directory(./src/cmd CMD_SRC_FILES)
aux_source_directory(./src/utils UTILS_SRC_FILES)
aux_source_directory(./src/wifi WIFI_SRC_FILES)
aux_source_directory(./src/middleware MIDDLEWARE_SRC_FILES)
# aux_source_directory(./src/net NET_SRC_FILES)
aux_source_directory(./src/system SYSTEM_SRC_FILES)
aux_source_directory(./src/libc LIBC_SRC_FILES)
aux_source_directory(./src/tone TONE_SRC_FILES)
aux_source_directory(./src/flash FLASH_SRC_FILES)
aux_source_directory(./src/player PLAYER_SRC_FILES)
aux_source_directory(./src/cloud/core CORE_SRC_FILES)
aux_source_directory(./src/cloud/proc PROC_SRC_FILES)
aux_source_directory(./src/cloud/recognizer REC_SRC_FILES)
aux_source_directory(./src/cloud/stream_text STREAM_SRC_FILES)
aux_source_directory(./src/cloud/mcp MCP_SRC_FILES)
aux_source_directory(./src/cloud CLOUD_SRC_FILES)
aux_source_directory(./src/cloud/mcp/tools MCP_TOOLS_SRC_FILES)

listenai_add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
    src/main.c
    src/app_algo.c
    src/app_client.c
    src/comm_service.c
    ${AUDIO_SRC_FILES}
    # ${CMD_SRC_FILES}
    ${UTILS_SRC_FILES}
    ${WIFI_SRC_FILES}
    ${MIDDLEWARE_SRC_FILES}
	${NET_SRC_FILES}
    ${SYSTEM_SRC_FILES}
    ${LIBC_SRC_FILES}
    ${TONE_SRC_FILES}
    ${FLASH_SRC_FILES}
    ${PLAYER_SRC_FILES}
    ${CORE_SRC_FILES}
    ${PROC_SRC_FILES}
    ${REC_SRC_FILES}
    ${STREAM_SRC_FILES}
    ${MCP_SRC_FILES}
    ${CLOUD_SRC_FILES}
    ${MCP_TOOLS_SRC_FILES}
)

target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--wrap,gettimeofday
    -Wl,--wrap,time
    -Wl,--wrap,gmtime_r
)

# add_subdirectory(src)
