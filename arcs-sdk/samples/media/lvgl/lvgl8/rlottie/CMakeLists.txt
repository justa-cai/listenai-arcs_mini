cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app)

if(DEFINED ENV{RLOTTIE_ROOT})
    set(RLOTTIE_ROOT_DEFAULT "$ENV{RLOTTIE_ROOT}")
else()
    set(RLOTTIE_ROOT_DEFAULT "${CMAKE_CURRENT_LIST_DIR}/rlottie-riscv")
endif()

set(RLOTTIE_ROOT "${RLOTTIE_ROOT_DEFAULT}" CACHE PATH "Path to prebuilt Samsung rlottie install")


find_path(RLOTTIE_INCLUDE_DIR
    NAMES rlottie_capi.h
    HINTS "${RLOTTIE_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(RLOTTIE_LIBRARY
    NAMES rlottie librlottie.a
    HINTS "${RLOTTIE_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT RLOTTIE_INCLUDE_DIR OR NOT RLOTTIE_LIBRARY)
    message(FATAL_ERROR
        "未在 RLOTTIE_ROOT='${RLOTTIE_ROOT}' 下找到可用的 Samsung rlottie 预编译产物。\n"
        "期望存在:\n"
        "  ${RLOTTIE_ROOT}/include/rlottie_capi.h\n"
        "  ${RLOTTIE_ROOT}/lib/librlottie.a")
endif()

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
)

# modules_lvgl8 会无条件编译 lv_port_indev.c；即便 sample 不启用触摸，
# 也需要让其在编译期能找到 lisa_touch.h。
get_filename_component(ARCS_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../" ABSOLUTE)
listenai_include_directories("${ARCS_SDK_ROOT}/drivers/lisa_touch")

# 使 modules_lvgl8 编译 lv_rlottie.c 时能够找到 Samsung rlottie 的 C API 头文件。
listenai_include_directories(${RLOTTIE_INCLUDE_DIR})

# 把 rlottie 放进 ListenAI 统一链接组，避免静态库顺序导致符号解析失败。
listenai_link_libraries("${RLOTTIE_LIBRARY}" m)

# rlottie 为 C++ 静态库，这里强制使用 C++ 链接器。
set_property(TARGET ${PROJECT_NAME} PROPERTY LINKER_LANGUAGE CXX)
