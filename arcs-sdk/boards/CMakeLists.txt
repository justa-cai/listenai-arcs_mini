# ARCS BOARD SUPPORT
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

# 1. 动态获取可用板型列表
function(get_available_boards output_var)
    set(boards_list "")
    file(GLOB board_dirs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
    foreach(item ${board_dirs})
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${item}/CMakeLists.txt")
                list(APPEND boards_list "${item}")
            endif()
        endif()
    endforeach()
    set(${output_var} ${boards_list} PARENT_SCOPE)
endfunction()

# 2. 获取板型名称
if(NOT DEFINED BOARD)
    # 动态生成板型列表
    get_available_boards(AVAILABLE_BOARDS)
    set(BOARD_LIST_STR "")
    foreach(board ${AVAILABLE_BOARDS})
        string(APPEND BOARD_LIST_STR "  - ${board}\n")
    endforeach()

    message(FATAL_ERROR
        "BOARD is not specified!\n"
        "Please specify a board using -DBOARD=<board_name>\n"
        "Available SDK boards:\n"
        "${BOARD_LIST_STR}\n"
        "Example usage:\n"
        "  ./build.sh -S samples/helloworld -DBOARD=arcs_evb\n"
        "  ./build.sh -S samples/helloworld -DBOARD=my_board -DBOARD_SEARCH_PATH=/path/to/boards")
endif()

message(STATUS "Target board: ${BOARD}")

# 3. 板型搜索（两级优先级）
set(BOARD_DIR "")

# 优先级1: 自定义板型路径 (如果 BOARD_SEARCH_PATH 已定义)
if(DEFINED BOARD_SEARCH_PATH)
    if(EXISTS "${BOARD_SEARCH_PATH}/${BOARD}")
        set(BOARD_DIR "${BOARD_SEARCH_PATH}/${BOARD}")
        message(STATUS "Board found (custom): ${BOARD} from ${BOARD_SEARCH_PATH}")
    else()
        message(WARNING "BOARD_SEARCH_PATH specified but board '${BOARD}' not found in ${BOARD_SEARCH_PATH}")
    endif()
endif()

# 优先级2: SDK 内置板型
if(NOT BOARD_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}")
        set(BOARD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}")
        message(STATUS "Board found (SDK): ${BOARD}")
    endif()
endif()

# 4. 验证板型目录存在
if(NOT BOARD_DIR)
    # 动态生成板型列表
    get_available_boards(AVAILABLE_BOARDS)
    set(BOARD_LIST_STR "")
    foreach(board ${AVAILABLE_BOARDS})
        string(APPEND BOARD_LIST_STR "  - ${board}\n")
    endforeach()

    message(FATAL_ERROR
        "Board '${BOARD}' not found!\n"
        "Searched in:\n"
        "  - ${BOARD_SEARCH_PATH}/${BOARD} (if BOARD_SEARCH_PATH defined)\n"
        "  - ${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}\n"
        "Please check:\n"
        "  1. BOARD variable is set correctly (-DBOARD=xxx)\n"
        "  2. Board directory exists\n"
        "  3. BOARD_SEARCH_PATH is correct (if using custom board)\n"
        "Available SDK boards:\n"
        "${BOARD_LIST_STR}\n"
        "Example usage:\n"
        "  ./build.sh -S samples/helloworld -DBOARD=arcs_evb\n"
        "  ./build.sh -S samples/helloworld -DBOARD=my_board -DBOARD_SEARCH_PATH=/path/to/boards")
endif()

# 5. 检查板型 CMakeLists.txt 存在
if(NOT EXISTS "${BOARD_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Board directory found but missing CMakeLists.txt: ${BOARD_DIR}\n"
        "Each board must contain a CMakeLists.txt file.\n"
        "Please refer to boards/BOARD_TEMPLATE.md for board structure requirements.")
endif()

# 6. 加载板型
# 使用第二个参数指定二进制目录，避免外部板型路径冲突
add_subdirectory(${BOARD_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${BOARD})

message(STATUS "Board loaded successfully: ${BOARD_DIR}")
