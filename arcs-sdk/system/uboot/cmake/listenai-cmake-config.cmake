if(NOT DEFINED ARCS_SDK_BASE)
    if(NOT DEFINED ENV{ARCS_BASE})
        message(FATAL_ERROR "ARCS_BASE is not defined")
    else()
        set(ARCS_SDK_BASE $ENV{ARCS_BASE})
    endif()
endif()

set(ARCS_SDK_CMAKE_PATH "${ARCS_SDK_BASE}/cmake")
set(LISTENAI_CMAKE_PATH "${ARCS_SDK_CMAKE_PATH}")

if(NOT DEFINED LISTENAI_TOOLS_PATH)
    if(NOT DEFINED ENV{LISTENAI_TOOLS_PATH})
        message(FATAL_ERROR "LISTENAI_TOOLS_PATH is not defined")
    else()
        set(LISTENAI_TOOLS_PATH $ENV{LISTENAI_TOOLS_PATH})
    endif()
endif()

string(REPLACE "\\" "/" LISTENAI_TOOLS_PATH "${LISTENAI_TOOLS_PATH}")
string(REPLACE "\\" "/" ARCS_SDK_BASE "${ARCS_SDK_BASE}")
string(REPLACE "\\" "/" ARCS_SDK_CMAKE_PATH "${ARCS_SDK_CMAKE_PATH}")

set(ENV{ARCS_BASE} "${ARCS_SDK_BASE}")

set(LISTENAI_TOOLS_MKHDR "${LISTENAI_TOOLS_PATH}/mkhdr/mkhdr")
set(LISTENAI_TOOLS_KCONFIG "${LISTENAI_TOOLS_PATH}/kconfig/kconfig")
set(LISTENAI_TOOLS_MENUCONFIG "${LISTENAI_TOOLS_PATH}/menuconfig/menuconfig")

option(LISTENAI_ADD_BIN_HEADR "kconfig no generate header of bin " ON)

if(NOT DEFINED LISTENAI_MODULES_DIR_LIST)
    set(LISTENAI_MODULES_DIR_LIST "")
endif()

function(check_tool tool_var tool_name)
    if(NOT DEFINED ${tool_var})
        message(FATAL_ERROR "${tool_name} is not defined")
    endif()

    if(NOT EXISTS "${${tool_var}}")
        message(FATAL_ERROR "Tool ${${tool_var}} does not exist")
    endif()

    string(REPLACE "\\" "/" ${tool_var} "${${tool_var}}")
    message(STATUS "Found ${tool_name}: ${${tool_var}}")
endfunction()

check_tool(LISTENAI_TOOLS_KCONFIG "ListenAI Kconfig tool")
check_tool(LISTENAI_TOOLS_MKHDR "ListenAI mkhdr tool")

include(${LISTENAI_CMAKE_PATH}/hex.cmake)
include(${LISTENAI_CMAKE_PATH}/version.cmake)

get_filename_component(UBOOT_CMAKE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include(${UBOOT_CMAKE_ROOT}/version.cmake)

function(listenai_find_modules dir)
    set(cmake_lists_path "${dir}/CMakeLists.txt")
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

if(CONFIG_BOOT_APP_CORE_AUTO)
    execute_process(
        COMMAND ${LISTENAI_TOOLS_MKHDR} -h
        OUTPUT_VARIABLE _mkhdr_help
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(FIND "${_mkhdr_help}" "-f " _mkhdr_has_f)
    if(_mkhdr_has_f EQUAL -1)
        message(WARNING
            "CONFIG_BOOT_APP_CORE_AUTO requires mkhdr with -f support.\n"
            "Current mkhdr: ${LISTENAI_TOOLS_MKHDR}\n"
            "Please update mkhdr to write target_core into the image header.")
    endif()
endif()

if (NOT DEFINED CHIP)
    set(CHIP arcs)
endif()

include(${LISTENAI_CMAKE_PATH}/${CHIP}-chip.cmake)
include(${LISTENAI_CMAKE_PATH}/${CHIP}-toolchain.cmake)
include(${LISTENAI_CMAKE_PATH}/common_compile_options.cmake)
include(${LISTENAI_CMAKE_PATH}/common_link_options.cmake)

listenai_include_directories(${CMAKE_BINARY_DIR}/generated/include)

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
