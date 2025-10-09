set(LISTENAI_CMAKE_PATH ${CMAKE_CURRENT_LIST_DIR})

if (NOT DEFINED LISTENAI_TOOLS_PATH)
    if (NOT DEFINED ENV{LISTENAI_TOOLS_PATH})
        message(FATAL_ERROR "LISTENAI_TOOLS_PATH is not defined")
    else()
        set(LISTENAI_TOOLS_PATH $ENV{LISTENAI_TOOLS_PATH})
    endif()
endif()

string(REPLACE "\\" "/" LISTENAI_TOOLS_PATH "${LISTENAI_TOOLS_PATH}")

set(LISTENAI_TOOLS_MKHDR ${LISTENAI_TOOLS_PATH}/mkhdr/mkhdr)
set(LISTENAI_TOOLS_KCONFIG ${LISTENAI_TOOLS_PATH}/kconfig/kconfig)
set(LISTENAI_TOOLS_MENUCONFIG ${LISTENAI_TOOLS_PATH}/menuconfig/menuconfig)

option(LISTENAI_ADD_BIN_HEADR "kconfig no generate header of bin " ON)

string(REPLACE "\\" "/" LISTENAI_CMAKE_PATH "${LISTENAI_CMAKE_PATH}")

if (NOT DEFINED LISTENAI_MODULES_DIR_LIST)
    message(WARNING "LISTENAI_MODULES_DIR_LIST is not defined")
    set(LISTENAI_MODULES_DIR_LIST "")
endif()

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
