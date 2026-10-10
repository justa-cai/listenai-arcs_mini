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
    peer_alsa.c
    peer_vidsink.c
    peer_vidsrc.c
    peer_v4l2.c
    ../core/sip_g711.c
    ../core/sip_jpeg_rtp.c
    ../core/sip_msg.c
    ../core/sip_sdp.c
    ../core/sip_rtp.c
    ../core/sip_ua.c
    ../ports/sip_osal_posix.c
)

target_include_directories(sip_peer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../ports
)

target_link_libraries(sip_peer PRIVATE asound pthread m jpeg)

target_compile_options(sip_peer PRIVATE -O2 -Wall -Wextra)

# LCD 回传接收器（调试工具，与设备侧 sip_screen.c 的自定义分片协议配套）：
#   ./tmp/sip-host/screen_sink [port=5070] [outdir=tmp/sip_host]
add_executable(screen_sink screen_sink.c)
target_compile_options(screen_sink PRIVATE -O2 -Wall -Wextra)
