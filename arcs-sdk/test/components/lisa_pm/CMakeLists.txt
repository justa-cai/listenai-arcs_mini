cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})
listenai_add_executable(${PROJECT_NAME})

# 测试源文件：runner + 共享 setup + 各 case
target_sources(${PROJECT_NAME} PRIVATE
    test_lisa_pm.c          # 主运行器（main + UNITY_BEGIN/END）
    test_common.c           # 公共环境 setup（WiFi + lisa_pm 低功耗）
    test_snapshot.c         # case：ILM/DLM 快照 canary
    test_lock.c             # case：系统睡眠锁引用计数
    test_wifi_lock.c        # case：WiFi 省电锁引用计数
    test_wakeup_cause.c     # case：唤醒原因归一化
    test_stats.c            # case：stats 自身正确性 (case 7a)
    test_lock_blocks_sleep.c # case：系统锁端到端抑制 sleep (case 7b)
    test_errors.c           # case：错误 / 边界路径 (case 3)
    test_sleep_callback.c   # case：应用睡眠回调 API 与 hook 顺序
    test_device_pm_attach.c # case：独立 PM attach registry
    # 添加新 case：在此追加 test_<name>.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE unity)

# 把 test_snapshot.c 中 .snap_dtcm_bss 段定向到 DTCM_BSS 输出, 让 case
# test_snapshot_dtcm_bss_canary 能验证 .dtcm.bss 也被 lisa_pm 注册并
# snapshot。否则 system.ld 的 .dtcm 输出段 glob (.dtcm .dtcm.*) 会
# 抢先匹配, 任何 .dtcm.bss.X 命名都会落到 .dtcm 段, 测不到 .dtcm.bss。
listenai_code_relocate(SECTIONS .snap_dtcm_bss LOCATION DTCM_BSS)

# 独立的 AP 固件子工程，与 CP 镜像一起构建
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
