set(LISTENAI_CMAKE_PATH ${CMAKE_CURRENT_LIST_DIR})

if (NOT DEFINED LISTENAI_TOOLS_PATH)
    if (NOT DEFINED ENV{LISTENAI_TOOLS_PATH})
        message(FATAL_ERROR "LISTENAI_TOOLS_PATH is not defined")
    else()
        set(LISTENAI_TOOLS_PATH $ENV{LISTENAI_TOOLS_PATH})
    endif()
endif()

if (NOT DEFINED ARCS_SDK_BASE)
    if (NOT DEFINED ENV{ARCS_BASE})
        message(FATAL_ERROR "ARCS_BASE is not defined")
    else()
        set(ARCS_SDK_BASE $ENV{ARCS_BASE})
    endif()
endif()

string(REPLACE "\\" "/" LISTENAI_TOOLS_PATH "${LISTENAI_TOOLS_PATH}")

set(LISTENAI_TOOLS_MKHDR ${LISTENAI_TOOLS_PATH}/mkhdr/mkhdr)
set(LISTENAI_TOOLS_KCONFIG ${LISTENAI_TOOLS_PATH}/kconfig/kconfig)
set(LISTENAI_TOOLS_MENUCONFIG ${LISTENAI_TOOLS_PATH}/menuconfig/menuconfig)

option(LISTENAI_ADD_BIN_HEADR "kconfig no generate header of bin " ON)

string(REPLACE "\\" "/" LISTENAI_CMAKE_PATH "${LISTENAI_CMAKE_PATH}")

if (NOT DEFINED LISTENAI_MODULES_DIR_LIST)
    message(STATUS "LISTENAI_MODULES_DIR_LIST is not defined")
    set(LISTENAI_MODULES_DIR_LIST "")
endif()

if(NOT LISTENAI_MODULES_DIR_LIST MATCHES "${ARCS_SDK_BASE}")
    list(APPEND LISTENAI_MODULES_DIR_LIST "${ARCS_SDK_BASE}")
endif()
message(STATUS "LISTENAI_MODULES_DIR_LIST: ${LISTENAI_MODULES_DIR_LIST}")

function(check_tool tool_var tool_name)
    if (NOT DEFINED ${tool_var})
        message(FATAL_ERROR "${tool_name} is not defined")
    endif()

    if (NOT EXISTS ${${tool_var}})
        message(FATAL_ERROR "Tool ${${tool_var}} does not exist")
    endif()

    string(REPLACE "\\" "/" ${tool_var} "${${tool_var}}")
    message(STATUS "Found ${tool_name}: ${${tool_var}}")
endfunction()

check_tool(LISTENAI_TOOLS_KCONFIG "ListenAI Kconfig tool")
check_tool(LISTENAI_TOOLS_MKHDR "ListenAI mkhdr tool")

include(${LISTENAI_CMAKE_PATH}/hex.cmake)

# 包含版本管理（在 kconfig 之前，因为 kconfig 可能依赖版本信息）
include(${LISTENAI_CMAKE_PATH}/version.cmake)

message(STATUS "Listenai module dir list: ${LISTENAI_MODULES_DIR_LIST}")

# 从指定的目录中查询模块,并将模块的路径存放到属性LISTENAI_MODULES中
function(listenai_find_modules dir)
    set(cmake_lists_path "${dir}/CMakeLists.txt")
    get_filename_component(DIRECTORY_NAME ${dir} NAME)
    if(EXISTS "${cmake_lists_path}")
        get_filename_component(module_abs_path "${dir}" ABSOLUTE)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_MODULES ${module_abs_path})
        message(STATUS "Found module: ${module_abs_path}")
        return()
    endif()

    file(GLOB CHILD_DIRS LIST_DIRECTORIES TRUE "${dir}/*")
    foreach(child_dir ${CHILD_DIRS})
        if(IS_DIRECTORY ${child_dir})
        listenai_find_modules(${child_dir})
        endif()
    endforeach()
endfunction()

foreach(dir ${LISTENAI_MODULES_DIR_LIST})
    listenai_find_modules(${dir})
endforeach()

# 板型搜索逻辑（在 Kconfig 解析之前）
# 设置 BOARD_KCONFIG_PATH 环境变量，供 boards/Kconfig 动态加载板型配置
set(BOARD_DIR "")

if(DEFINED BOARD)
    # 优先级1: 自定义板型路径
    if(DEFINED BOARD_SEARCH_PATH)
        if(EXISTS "${BOARD_SEARCH_PATH}/${BOARD}")
            set(BOARD_DIR "${BOARD_SEARCH_PATH}/${BOARD}")
        endif()
    endif()

    # 优先级2: SDK 内置板型
    if(NOT BOARD_DIR)
        if(EXISTS "${ARCS_SDK_BASE}/boards/${BOARD}")
            set(BOARD_DIR "${ARCS_SDK_BASE}/boards/${BOARD}")
        endif()
    endif()
endif()

# 设置板型 Kconfig 路径环境变量
# 如果找到板型，设置为板型的 Kconfig 文件路径
# 如果未找到，设置为不存在的路径，让 osource 静默跳过（避免 Kconfig 解析错误）
if(BOARD_DIR AND EXISTS "${BOARD_DIR}/Kconfig")
    set(ENV{BOARD_KCONFIG_PATH} "${BOARD_DIR}/Kconfig")
else()
    # 设置为一个明确不存在的文件路径，让 osource 静默跳过
    set(ENV{BOARD_KCONFIG_PATH} "${ARCS_SDK_BASE}/boards/.kconfig.not.found")
endif()

include(${LISTENAI_CMAKE_PATH}/kconfig.cmake)
include(${LISTENAI_CMAKE_PATH}/extensions.cmake)

if (NOT DEFINED CHIP)
    set(CHIP arcs)
endif()

include(${LISTENAI_CMAKE_PATH}/${CHIP}-chip.cmake)
include(${LISTENAI_CMAKE_PATH}/${CHIP}-toolchain.cmake)
include(${LISTENAI_CMAKE_PATH}/common_compile_options.cmake)
include(${LISTENAI_CMAKE_PATH}/common_link_options.cmake)

listenai_include_directories(${CMAKE_BINARY_DIR}/generated/include)
listenai_library_named(_inner_app)
listenai_library_sources(${LISTENAI_CMAKE_PATH}/empty.c)

# 生成 SDK 版本头文件
execute_process(
    COMMAND ${CMAKE_COMMAND}
        -DARCS_SDK_BASE=${ARCS_SDK_BASE}
        -DOUT_FILE=${CMAKE_BINARY_DIR}/generated/include/sdk_version.h
        -DSDK_VERSION_MAJOR=${SDK_VERSION_MAJOR}
        -DSDK_VERSION_MINOR=${SDK_VERSION_MINOR}
        -DSDK_PATCHLEVEL=${SDK_PATCHLEVEL}
        -DSDK_VERSION_STRING=${SDK_VERSION_STRING}
        -DSDK_VERSION_CODE=${SDK_VERSION_CODE}
        -DSDK_VERSION_NUMBER=${SDK_VERSION_NUMBER}
        -DSDKVERSION=${SDKVERSION}
        -P ${ARCS_SDK_BASE}/cmake/gen_version_h.cmake
    WORKING_DIRECTORY ${ARCS_SDK_BASE}
)
