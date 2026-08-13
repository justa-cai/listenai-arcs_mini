cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(game_nes)

listenai_add_executable(${PROJECT_NAME})

# 让 NES_LOG 中的 __FILE__ 输出更友好：默认裁剪掉工程根路径，变成相对路径。
# 注：NES_LOG_PATH_PREFIX 必须是字符串字面量（带结尾 '/').
set(_nes_log_prefix "${CMAKE_SOURCE_DIR}/")
if(DEFINED ENV{ARCS_BASE} AND NOT "$ENV{ARCS_BASE}" STREQUAL "")
	set(_nes_log_prefix "$ENV{ARCS_BASE}/")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE NES_LOG_PATH_PREFIX=\"${_nes_log_prefix}\")

file(GLOB NES_CORE_SOURCES
	src/nes/core/*.c
	src/nes/core/nes_mapper/*.c
	src/nes/core/nes_mapper/infones/*.c
)

target_sources(${PROJECT_NAME} PRIVATE
	src/main.c
	src/game_nes_usb_keyboard.c
	src/nes/port/nes_port.c
	${NES_CORE_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
	src
	src/nes/inc
	src/nes/port
)

listenai_include_directories(
	src
)

set_source_files_properties(${NES_CORE_SOURCES} src/nes/port/nes_port.c src/main.c
	PROPERTIES COMPILE_FLAGS "-O3 -fno-strict-aliasing"
)

if(CONFIG_GAME_NES_USB_HOST_KEYBOARD)
	listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)
endif()
