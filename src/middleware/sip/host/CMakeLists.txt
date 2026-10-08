# 主机侧构建：Ubuntu 对端程序 sip_peer，与设备固件共用 ../core 的同一份 .c。
#
#   cmake -S src/middleware/sip/host -B tmp/sip-host
#   cmake --build tmp/sip-host
#   ./tmp/sip-host/sip_peer            # 查看用法
#
# 产物落在调用方指定的目录（建议 tmp/），不污染源码树。
cmake_minimum_required(VERSION 3.10)
project(sip_peer C)

set(CMAKE_C_STANDARD 11)

add_executable(sip_peer
    sip_peer.c
    ../core/sip_g711.c
    ../ports/sip_osal_posix.c
)

target_include_directories(sip_peer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports
)

target_compile_options(sip_peer PRIVATE -O2 -Wall -Wextra)
