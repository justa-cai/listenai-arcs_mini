# SPDX-License-Identifier: Apache-2.0
# ARCS SDK 版本头文件生成脚本
# 此脚本由主 CMakeLists.txt 通过 execute_process 调用

# 尝试获取 git 提交哈希
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS ${ARCS_SDK_BASE}/.git)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --abbrev=12 --always
        WORKING_DIRECTORY ${ARCS_SDK_BASE}
        OUTPUT_VARIABLE BUILD_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

# 如果未获取到 git 信息，设置为默认值
if(NOT BUILD_VERSION)
    set(BUILD_VERSION "unknown")
endif()

# 从模板生成头文件
configure_file(${ARCS_SDK_BASE}/sdk_version.h.in ${OUT_FILE} @ONLY)

message(STATUS "生成版本头文件: ${OUT_FILE}")
message(STATUS "  BUILD_VERSION: ${BUILD_VERSION}")
