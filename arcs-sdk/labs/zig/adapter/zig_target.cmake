# zig_target.cmake — On-demand Zig build helpers for ARCS SDK targets
#
# This file implements the target-facing part of the centralized Zig flow.
# It is loaded once by `labs/zig/adapter/CMakeLists.txt`.
#
# Target contract:
#   set(LISTENAI_ZIG_APP ${CMAKE_CURRENT_SOURCE_DIR}/src/zig_app.zig)
#   listenai_add_executable(${PROJECT_NAME})
#   listenai_maybe_enable_zig_target(${PROJECT_NAME})
#
# When `LISTENAI_ZIG_APP` is present, the helper will:
#   1. Find the host Zig compiler on demand
#   2. Build the shared `arcs-zig` adapter library
#   3. Compile the target's `.zig` file to a `.o` object
#   4. Link both the adapter and the object into the target
#
# See also: `labs/zig/usage.rst`

cmake_policy(PUSH)
if(POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()

function(_arcs_zig_split_assignment ENTRY OUT_DECL OUT_VALUE)
    string(FIND "${ENTRY}" "=" EQUAL_INDEX)
    if(EQUAL_INDEX LESS 1)
        message(FATAL_ERROR "Expected Zig assignment in the form <name>=<value>, got: ${ENTRY}")
    endif()

    string(SUBSTRING "${ENTRY}" 0 ${EQUAL_INDEX} ZIG_DECL)
    math(EXPR VALUE_INDEX "${EQUAL_INDEX} + 1")
    string(SUBSTRING "${ENTRY}" ${VALUE_INDEX} -1 ZIG_VALUE)

    if("${ZIG_VALUE}" STREQUAL "")
        message(FATAL_ERROR "Expected non-empty value for Zig assignment: ${ENTRY}")
    endif()

    set(${OUT_DECL} "${ZIG_DECL}" PARENT_SCOPE)
    set(${OUT_VALUE} "${ZIG_VALUE}" PARENT_SCOPE)
endfunction()

function(_arcs_zig_resolve_cmake_value OUT_VALUE VALUE_OR_VARIABLE)
    if(DEFINED "${VALUE_OR_VARIABLE}")
        set(${OUT_VALUE} "${${VALUE_OR_VARIABLE}}" PARENT_SCOPE)
    else()
        set(${OUT_VALUE} "${VALUE_OR_VARIABLE}" PARENT_SCOPE)
    endif()
endfunction()

function(_arcs_zig_resolve_bool_literal OUT_VALUE VALUE_OR_VARIABLE)
    if(DEFINED "${VALUE_OR_VARIABLE}")
        set(BOOL_VALUE "${${VALUE_OR_VARIABLE}}")
    elseif("${VALUE_OR_VARIABLE}" MATCHES "^CONFIG_")
        set(BOOL_VALUE false)
    else()
        set(BOOL_VALUE "${VALUE_OR_VARIABLE}")
    endif()

    if(BOOL_VALUE)
        set(${OUT_VALUE} true PARENT_SCOPE)
    else()
        set(${OUT_VALUE} false PARENT_SCOPE)
    endif()
endfunction()

function(_arcs_zig_resolve_string_literal OUT_VALUE VALUE_OR_VARIABLE)
    if(DEFINED "${VALUE_OR_VARIABLE}")
        set(${OUT_VALUE} "${${VALUE_OR_VARIABLE}}" PARENT_SCOPE)
    elseif("${VALUE_OR_VARIABLE}" MATCHES "^CONFIG_")
        message(FATAL_ERROR "Undefined Kconfig string for Zig config: ${VALUE_OR_VARIABLE}")
    else()
        set(${OUT_VALUE} "${VALUE_OR_VARIABLE}" PARENT_SCOPE)
    endif()
endfunction()

function(_arcs_zig_escape_string OUT_VALUE RAW_VALUE)
    string(REPLACE "\\" "\\\\" ESCAPED_VALUE "${RAW_VALUE}")
    string(REPLACE "\"" "\\\"" ESCAPED_VALUE "${ESCAPED_VALUE}")
    string(REPLACE "\n" "\\n" ESCAPED_VALUE "${ESCAPED_VALUE}")
    string(REPLACE "\r" "\\r" ESCAPED_VALUE "${ESCAPED_VALUE}")
    set(${OUT_VALUE} "${ESCAPED_VALUE}" PARENT_SCOPE)
endfunction()

function(_arcs_zig_append_config_const OUT_CONTENT ZIG_DECL ZIG_LITERAL)
    set(CONTENT "${${OUT_CONTENT}}pub const ${ZIG_DECL} = ${ZIG_LITERAL};\n")
    set(${OUT_CONTENT} "${CONTENT}" PARENT_SCOPE)
endfunction()

function(_arcs_zig_append_absolute_include OUT_LIST INCLUDE_DIR)
    if(IS_ABSOLUTE "${INCLUDE_DIR}")
        set(RESOLVED_DIR "${INCLUDE_DIR}")
    else()
        get_filename_component(RESOLVED_DIR "${INCLUDE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    set(UPDATED_LIST ${${OUT_LIST}})
    list(APPEND UPDATED_LIST "-I${RESOLVED_DIR}")
    set(${OUT_LIST} ${UPDATED_LIST} PARENT_SCOPE)
endfunction()

function(listenai_zig_register_module)
    cmake_parse_arguments(ZIG_MODULE "" "NAME;ROOT" "DEPENDS;C_INCLUDE_DIRS;MODULE_DEPS" ${ARGN})
    if(ZIG_MODULE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown listenai_zig_register_module arguments: ${ZIG_MODULE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ZIG_MODULE_NAME)
        message(FATAL_ERROR "listenai_zig_register_module requires NAME")
    endif()
    if(NOT ZIG_MODULE_ROOT)
        message(FATAL_ERROR "listenai_zig_register_module requires ROOT for module '${ZIG_MODULE_NAME}'")
    endif()
    if("${ZIG_MODULE_NAME}" STREQUAL "root" OR "${ZIG_MODULE_NAME}" STREQUAL "arcs")
        message(FATAL_ERROR "Zig module name '${ZIG_MODULE_NAME}' is reserved")
    endif()
    if(NOT IS_ABSOLUTE "${ZIG_MODULE_ROOT}")
        get_filename_component(ZIG_MODULE_ROOT "${ZIG_MODULE_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    if(NOT EXISTS "${ZIG_MODULE_ROOT}")
        message(FATAL_ERROR "Zig module '${ZIG_MODULE_NAME}' root does not exist: ${ZIG_MODULE_ROOT}")
    endif()

    get_property(REGISTERED_MODULES GLOBAL PROPERTY ARCS_ZIG_REGISTERED_MODULES)
    if(REGISTERED_MODULES)
        list(FIND REGISTERED_MODULES "${ZIG_MODULE_NAME}" MODULE_INDEX)
    else()
        set(MODULE_INDEX -1)
    endif()

    if(MODULE_INDEX GREATER -1)
        get_property(EXISTING_ROOT GLOBAL PROPERTY "ARCS_ZIG_MODULE_${ZIG_MODULE_NAME}_ROOT")
        if(NOT "${EXISTING_ROOT}" STREQUAL "${ZIG_MODULE_ROOT}")
            message(FATAL_ERROR
                "Zig module '${ZIG_MODULE_NAME}' is already registered with root '${EXISTING_ROOT}', "
                "cannot re-register with '${ZIG_MODULE_ROOT}'"
            )
        endif()
    else()
        list(APPEND REGISTERED_MODULES "${ZIG_MODULE_NAME}")
        set_property(GLOBAL PROPERTY ARCS_ZIG_REGISTERED_MODULES "${REGISTERED_MODULES}")
    endif()

    set_property(GLOBAL PROPERTY "ARCS_ZIG_MODULE_${ZIG_MODULE_NAME}_ROOT" "${ZIG_MODULE_ROOT}")
    set_property(GLOBAL PROPERTY "ARCS_ZIG_MODULE_${ZIG_MODULE_NAME}_DEPENDS" "${ZIG_MODULE_DEPENDS}")
    set_property(GLOBAL PROPERTY "ARCS_ZIG_MODULE_${ZIG_MODULE_NAME}_C_INCLUDE_DIRS" "${ZIG_MODULE_C_INCLUDE_DIRS}")
    set_property(GLOBAL PROPERTY "ARCS_ZIG_MODULE_${ZIG_MODULE_NAME}_MODULE_DEPS" "${ZIG_MODULE_MODULE_DEPS}")
endfunction()

function(_arcs_zig_collect_registered_module MODULE_NAME OUT_NAMES OUT_MODULES OUT_DEPENDS OUT_INCLUDE_DIRS)
    set(COLLECTED_NAMES ${${OUT_NAMES}})
    if(COLLECTED_NAMES)
        list(FIND COLLECTED_NAMES "${MODULE_NAME}" MODULE_INDEX)
    else()
        set(MODULE_INDEX -1)
    endif()
    if(MODULE_INDEX GREATER -1)
        return()
    endif()

    get_property(MODULE_ROOT GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_ROOT")
    if(NOT MODULE_ROOT)
        get_property(PENDING_ROOT GLOBAL PROPERTY "ARCS_ZIG_PENDING_MODULE_${MODULE_NAME}_ROOT")
        if(PENDING_ROOT)
            get_property(PENDING_DEPENDS GLOBAL PROPERTY "ARCS_ZIG_PENDING_MODULE_${MODULE_NAME}_DEPENDS")
            get_property(PENDING_INCLUDE_DIRS GLOBAL PROPERTY "ARCS_ZIG_PENDING_MODULE_${MODULE_NAME}_C_INCLUDE_DIRS")
            get_property(PENDING_MODULE_DEPS GLOBAL PROPERTY "ARCS_ZIG_PENDING_MODULE_${MODULE_NAME}_MODULE_DEPS")
            listenai_zig_register_module(
                NAME ${MODULE_NAME}
                ROOT ${PENDING_ROOT}
                DEPENDS ${PENDING_DEPENDS}
                C_INCLUDE_DIRS ${PENDING_INCLUDE_DIRS}
                MODULE_DEPS ${PENDING_MODULE_DEPS}
            )
            get_property(MODULE_ROOT GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_ROOT")
        endif()
    endif()
    if(NOT MODULE_ROOT)
        message(FATAL_ERROR "Zig module '${MODULE_NAME}' is not registered")
    endif()
    get_property(MODULE_DEPENDS GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_DEPENDS")
    get_property(MODULE_INCLUDE_DIRS GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_C_INCLUDE_DIRS")
    get_property(MODULE_DEPS GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_MODULE_DEPS")

    list(APPEND COLLECTED_NAMES "${MODULE_NAME}")
    set(${OUT_NAMES} ${COLLECTED_NAMES})
    set(${OUT_NAMES} ${COLLECTED_NAMES} PARENT_SCOPE)

    set(COLLECTED_MODULES ${${OUT_MODULES}})
    list(APPEND COLLECTED_MODULES "${MODULE_NAME}=${MODULE_ROOT}")
    set(${OUT_MODULES} ${COLLECTED_MODULES})
    set(${OUT_MODULES} ${COLLECTED_MODULES} PARENT_SCOPE)

    set(COLLECTED_DEPENDS ${${OUT_DEPENDS}})
    list(APPEND COLLECTED_DEPENDS ${MODULE_ROOT} ${MODULE_DEPENDS})
    set(${OUT_DEPENDS} ${COLLECTED_DEPENDS})
    set(${OUT_DEPENDS} ${COLLECTED_DEPENDS} PARENT_SCOPE)

    set(COLLECTED_INCLUDE_DIRS ${${OUT_INCLUDE_DIRS}})
    list(APPEND COLLECTED_INCLUDE_DIRS ${MODULE_INCLUDE_DIRS})
    set(${OUT_INCLUDE_DIRS} ${COLLECTED_INCLUDE_DIRS})
    set(${OUT_INCLUDE_DIRS} ${COLLECTED_INCLUDE_DIRS} PARENT_SCOPE)

    foreach(DEP_MODULE ${MODULE_DEPS})
        _arcs_zig_collect_registered_module(
            "${DEP_MODULE}"
            ${OUT_NAMES}
            ${OUT_MODULES}
            ${OUT_DEPENDS}
            ${OUT_INCLUDE_DIRS}
        )
    endforeach()

    # Recursive calls update this function's scope first; forward the final
    # transitive module set back to the original caller.
    set(${OUT_NAMES} ${${OUT_NAMES}} PARENT_SCOPE)
    set(${OUT_MODULES} ${${OUT_MODULES}} PARENT_SCOPE)
    set(${OUT_DEPENDS} ${${OUT_DEPENDS}} PARENT_SCOPE)
    set(${OUT_INCLUDE_DIRS} ${${OUT_INCLUDE_DIRS}} PARENT_SCOPE)
endfunction()

function(zig_write_config_module OUT_FILE)
    cmake_parse_arguments(ZIG_CONFIG "" "" "BOOL;INT;UINT;STRING;VALUE" ${ARGN})
    if(ZIG_CONFIG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown zig_write_config_module arguments: ${ZIG_CONFIG_UNPARSED_ARGUMENTS}")
    endif()

    set(CONFIG_CONTENT "// Generated by zig_write_config_module(); do not edit.\n\n")

    foreach(ENTRY ${ZIG_CONFIG_BOOL})
        _arcs_zig_split_assignment("${ENTRY}" ZIG_DECL ZIG_VALUE)
        _arcs_zig_resolve_bool_literal(ZIG_BOOL_LITERAL "${ZIG_VALUE}")
        _arcs_zig_append_config_const(CONFIG_CONTENT "${ZIG_DECL}" "${ZIG_BOOL_LITERAL}")
    endforeach()

    foreach(ENTRY ${ZIG_CONFIG_INT})
        _arcs_zig_split_assignment("${ENTRY}" ZIG_DECL ZIG_VALUE)
        _arcs_zig_resolve_cmake_value(RESOLVED_VALUE "${ZIG_VALUE}")
        if(NOT "${RESOLVED_VALUE}" MATCHES "^[-+]?([0-9]+|0[xX][0-9A-Fa-f]+)$")
            message(FATAL_ERROR "Expected integer Zig config value for ${ZIG_DECL}, got: ${RESOLVED_VALUE}")
        endif()
        _arcs_zig_append_config_const(CONFIG_CONTENT "${ZIG_DECL}" "${RESOLVED_VALUE}")
    endforeach()

    foreach(ENTRY ${ZIG_CONFIG_UINT})
        _arcs_zig_split_assignment("${ENTRY}" ZIG_DECL ZIG_VALUE)
        _arcs_zig_resolve_cmake_value(RESOLVED_VALUE "${ZIG_VALUE}")
        if(NOT "${RESOLVED_VALUE}" MATCHES "^([0-9]+|0[xX][0-9A-Fa-f]+)$")
            message(FATAL_ERROR "Expected unsigned integer Zig config value for ${ZIG_DECL}, got: ${RESOLVED_VALUE}")
        endif()
        _arcs_zig_append_config_const(CONFIG_CONTENT "${ZIG_DECL}" "${RESOLVED_VALUE}")
    endforeach()

    foreach(ENTRY ${ZIG_CONFIG_STRING})
        _arcs_zig_split_assignment("${ENTRY}" ZIG_DECL ZIG_VALUE)
        _arcs_zig_resolve_string_literal(RESOLVED_VALUE "${ZIG_VALUE}")
        _arcs_zig_escape_string(ESCAPED_VALUE "${RESOLVED_VALUE}")
        _arcs_zig_append_config_const(CONFIG_CONTENT "${ZIG_DECL}" "\"${ESCAPED_VALUE}\"")
    endforeach()

    foreach(ENTRY ${ZIG_CONFIG_VALUE})
        _arcs_zig_split_assignment("${ENTRY}" ZIG_DECL ZIG_VALUE)
        _arcs_zig_append_config_const(CONFIG_CONTENT "${ZIG_DECL}" "${ZIG_VALUE}")
    endforeach()

    file(WRITE "${OUT_FILE}" "${CONFIG_CONTENT}")
endfunction()

function(arcs_zig_ensure_toolchain)
    if(TARGET zig_build)
        return()
    endif()

    get_property(ARCS_ZIG_DIR GLOBAL PROPERTY ARCS_ZIG_ADAPTER_DIR)
    get_property(ARCS_ZIG_SDK_ROOT GLOBAL PROPERTY ARCS_ZIG_SDK_ROOT)

    if(NOT ARCS_ZIG_DIR)
        message(FATAL_ERROR "ARCS Zig adapter is not registered. Ensure labs/zig/adapter is added before enabling Zig target sources.")
    endif()

    find_program(ZIG_COMPILER zig)
    if(NOT ZIG_COMPILER)
        message(FATAL_ERROR "Zig compiler not found. Install from https://ziglang.org/download/")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ZIG_OPTIMIZE "Debug")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(ZIG_OPTIMIZE "ReleaseSafe")
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(ZIG_OPTIMIZE "ReleaseSmall")
    else()
        set(ZIG_OPTIMIZE "ReleaseSafe")
    endif()

    if(CONFIG_FPU)
        set(ARCS_ZIG_TARGET "riscv32-freestanding-eabihf")
        set(ARCS_ZIG_CPU "baseline_rv32+a+c+m+f-d")
        set(ARCS_ZIG_FPU true)
    else()
        set(ARCS_ZIG_TARGET "riscv32-freestanding-none")
        set(ARCS_ZIG_CPU "baseline_rv32+a+c+m-f-d")
        set(ARCS_ZIG_FPU false)
    endif()
    set_property(GLOBAL PROPERTY ARCS_ZIG_TARGET ${ARCS_ZIG_TARGET})
    set_property(GLOBAL PROPERTY ARCS_ZIG_CPU ${ARCS_ZIG_CPU})

    set(ZIG_BUILD_DIR ${CMAKE_BINARY_DIR}/zig-out)
    set(ZIG_CACHE_DIR ${CMAKE_BINARY_DIR}/zig-cache)
    set(ZIG_GLOBAL_CACHE_DIR ${CMAKE_BINARY_DIR}/zig-global-cache)

    add_custom_command(
        OUTPUT ${ZIG_BUILD_DIR}/lib/libarcs-zig.a
        COMMAND ${ZIG_COMPILER} build
            --cache-dir ${ZIG_CACHE_DIR}
            --global-cache-dir ${ZIG_GLOBAL_CACHE_DIR}
            -Doptimize=${ZIG_OPTIMIZE}
            -Dsdk-root=${ARCS_ZIG_SDK_ROOT}
            -Dgenerated-include=${CMAKE_BINARY_DIR}/generated/include
            -Dfpu=${ARCS_ZIG_FPU}
            --prefix ${ZIG_BUILD_DIR}
            --prefix-exe-dir lib
            --prefix-lib-dir lib
        WORKING_DIRECTORY ${ARCS_ZIG_DIR}
        COMMENT "Building ARCS Zig adapter library"
        DEPENDS
            ${ARCS_ZIG_DIR}/src/root.zig
            ${ARCS_ZIG_DIR}/src/bindings/lvgl.zig
            ${ARCS_ZIG_DIR}/include/arcs_zig_lvgl_config.h
            ${ARCS_ZIG_DIR}/build.zig
    )

    add_custom_target(zig_build
        DEPENDS ${ZIG_BUILD_DIR}/lib/libarcs-zig.a
    )

    add_dependencies(arcs-zig zig_build)
    target_link_directories(arcs-zig INTERFACE ${ZIG_BUILD_DIR}/lib)
    target_link_libraries(arcs-zig INTERFACE ${ZIG_BUILD_DIR}/lib/libarcs-zig.a)
endfunction()

function(zig_add_target_source TARGET ZIG_SOURCE)
    cmake_parse_arguments(ZIG_TARGET "" "" "DEPENDS;MODULES;USE_REGISTERED_MODULES;C_INCLUDE_DIRS" ${ARGN})
    if(ZIG_TARGET_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown zig_add_target_source arguments: ${ZIG_TARGET_UNPARSED_ARGUMENTS}")
    endif()

    set(REGISTERED_MODULE_NAMES)
    set(REGISTERED_MODULES)
    set(REGISTERED_DEPENDS)
    set(REGISTERED_INCLUDE_DIRS)
    foreach(MODULE_NAME ${ZIG_TARGET_USE_REGISTERED_MODULES})
        _arcs_zig_collect_registered_module(
            "${MODULE_NAME}"
            REGISTERED_MODULE_NAMES
            REGISTERED_MODULES
            REGISTERED_DEPENDS
            REGISTERED_INCLUDE_DIRS
        )
    endforeach()

    arcs_zig_ensure_toolchain()

    get_property(ARCS_ZIG_DIR GLOBAL PROPERTY ARCS_ZIG_ADAPTER_DIR)
    get_property(ARCS_ZIG_SDK_ROOT GLOBAL PROPERTY ARCS_ZIG_SDK_ROOT)
    get_property(ARCS_ZIG_TARGET GLOBAL PROPERTY ARCS_ZIG_TARGET)
    get_property(ARCS_ZIG_CPU GLOBAL PROPERTY ARCS_ZIG_CPU)
    set(ARCS_ROOT_ZIG ${ARCS_ZIG_DIR}/src/root.zig)

    get_filename_component(ZIG_BASENAME ${ZIG_SOURCE} NAME_WE)
    set(ZIG_OBJ ${CMAKE_CURRENT_BINARY_DIR}/${ZIG_BASENAME}.o)

    set(SDK_INCLUDE_ARGS "")
    set(SDK_INCLUDE_DIRS
        labs/zig/adapter/include
        modules/freertos/include
        system/os/inc
        system/log
        system/init
        system/console
        system/heap
        system/ringbuf
        drivers/lisa_device
        drivers/lisa_gpio
        drivers/lisa_uart
        drivers/lisa_i2c
        drivers/lisa_spi
        drivers/lisa_adc
        drivers/lisa_pwm
        drivers/lisa_flash
        drivers/lisa_display
        drivers/lisa_camera
        drivers/lisa_audio
        drivers/lisa_i2s
        drivers/lisa_rtc
        drivers/lisa_sdmmc
        drivers/lisa_touch
        soc/arcs
        soc/common
        soc/arcs/hal/chip/arcs/include
        modules/wifi_manager/include
        modules/mac_manager/src
        modules/httpclient/include
        modules/httpclient/include/API
        modules/http_ssl/include
        modules/lvgl8
        modules/lvgl8/src
        modules/mbedtls/mbedtls/include
        modules/mbedtls/configs
        modules/mbedtls/port/mem
        modules/mbedtls/port/platform
        components/app_player
        components/lisa_bluetooth
        components/lisa_kv
        components/lisa_wifi
        soc/arcs/hal/chip/arcs/bt_hal
        soc/arcs/hal/chip/arcs/btos
        soc/arcs/hal/chip/arcs/btos/task/api
        soc/arcs/hal/chip/arcs/wcnd/include/bt_inc
        soc/arcs/hal/modules/profiles/netcfg_ble/netcfg_bles/api
        soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/src/include
        soc/arcs/hal/chip/arcs/lwip/lwip-2.2.1/contrib/ports/rtos/include
        soc/arcs/hal/chip/arcs/lwip/port/include
        soc/arcs/hal/chip/arcs/rtos/rtos_al
        soc/arcs/hal/chip/arcs/include/net
        soc/arcs/hal/chip/arcs/include/wifi
    )

    foreach(DIR ${SDK_INCLUDE_DIRS})
        list(APPEND SDK_INCLUDE_ARGS "-I${ARCS_ZIG_SDK_ROOT}/${DIR}")
    endforeach()
    list(APPEND SDK_INCLUDE_ARGS "-I${CMAKE_BINARY_DIR}/generated/include")

    foreach(DIR ${REGISTERED_INCLUDE_DIRS} ${ZIG_TARGET_C_INCLUDE_DIRS})
        _arcs_zig_append_absolute_include(SDK_INCLUDE_ARGS "${DIR}")
    endforeach()

    foreach(DIR ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
        if(IS_DIRECTORY "${DIR}")
            list(APPEND SDK_INCLUDE_ARGS "-isystem" "${DIR}")
        endif()
    endforeach()

    set(ZIG_MODULE_DEPS "")
    set(ZIG_MODULE_DEP_ARGS "")
    set(ZIG_MODULE_MAP_ARGS "")
    set(ALL_ZIG_MODULES ${ZIG_TARGET_MODULES} ${REGISTERED_MODULES})
    foreach(MODULE_ENTRY ${ZIG_TARGET_MODULES})
        _arcs_zig_split_assignment("${MODULE_ENTRY}" MODULE_NAME MODULE_PATH)
        if("${MODULE_NAME}" STREQUAL "root" OR "${MODULE_NAME}" STREQUAL "arcs")
            message(FATAL_ERROR "Zig module name '${MODULE_NAME}' is reserved")
        endif()
        list(FIND REGISTERED_MODULE_NAMES "${MODULE_NAME}" REGISTERED_NAME_INDEX)
        if(REGISTERED_NAME_INDEX GREATER -1)
            message(FATAL_ERROR
                "Zig module '${MODULE_NAME}' is both passed through MODULES and USE_REGISTERED_MODULES. "
                "Use only one source for each module name."
            )
        endif()
    endforeach()
    foreach(MODULE_ENTRY ${ALL_ZIG_MODULES})
        _arcs_zig_split_assignment("${MODULE_ENTRY}" MODULE_NAME MODULE_PATH)
        if("${MODULE_NAME}" STREQUAL "root" OR "${MODULE_NAME}" STREQUAL "arcs")
            message(FATAL_ERROR "Zig module name '${MODULE_NAME}' is reserved")
        endif()

        list(APPEND ZIG_MODULE_DEP_ARGS "--dep" "${MODULE_NAME}")
        list(APPEND ZIG_MODULE_MAP_ARGS "--dep" "arcs")
        set(MODULE_DEPS)
        list(FIND REGISTERED_MODULE_NAMES "${MODULE_NAME}" REGISTERED_NAME_INDEX)
        if(REGISTERED_NAME_INDEX GREATER -1)
            get_property(MODULE_DEPS GLOBAL PROPERTY "ARCS_ZIG_MODULE_${MODULE_NAME}_MODULE_DEPS")
        endif()
        foreach(DEP_MODULE ${MODULE_DEPS})
            list(APPEND ZIG_MODULE_MAP_ARGS "--dep" "${DEP_MODULE}")
        endforeach()
        list(APPEND ZIG_MODULE_MAP_ARGS "-M${MODULE_NAME}=${MODULE_PATH}" ${SDK_INCLUDE_ARGS})
        list(APPEND ZIG_MODULE_DEPS "${MODULE_PATH}")
    endforeach()
    list(APPEND ZIG_MODULE_DEPS ${REGISTERED_DEPENDS})
    if(ZIG_MODULE_DEPS)
        list(REMOVE_DUPLICATES ZIG_MODULE_DEPS)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ZIG_OPT "Debug")
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(ZIG_OPT "ReleaseSmall")
    else()
        set(ZIG_OPT "ReleaseSafe")
    endif()

    add_custom_command(
        OUTPUT ${ZIG_OBJ}
        COMMAND ${ZIG_COMPILER} build-obj
            -O${ZIG_OPT}
            -target ${ARCS_ZIG_TARGET}
            -mcpu=${ARCS_ZIG_CPU}
            --dep arcs
            ${ZIG_MODULE_DEP_ARGS}
            -Mroot=${ZIG_SOURCE}
            ${SDK_INCLUDE_ARGS}
            -Marcs=${ARCS_ROOT_ZIG}
            ${SDK_INCLUDE_ARGS}
            ${ZIG_MODULE_MAP_ARGS}
            -femit-bin=${ZIG_OBJ}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${ZIG_SOURCE} ${ARCS_ROOT_ZIG} ${ZIG_MODULE_DEPS} ${ZIG_TARGET_DEPENDS} zig_build
        COMMENT "Compiling Zig target source: ${ZIG_BASENAME}.zig"
    )

    add_custom_target(${TARGET}_zig_target DEPENDS ${ZIG_OBJ})
    add_dependencies(${TARGET} ${TARGET}_zig_target)

    target_link_libraries(${TARGET} PRIVATE ${ZIG_OBJ})
endfunction()

function(listenai_maybe_enable_zig_target TARGET)
    cmake_parse_arguments(ZIG_TARGET "" "" "DEPENDS;MODULES;USE_REGISTERED_MODULES;C_INCLUDE_DIRS" ${ARGN})
    if(ZIG_TARGET_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown listenai_maybe_enable_zig_target arguments: ${ZIG_TARGET_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT LISTENAI_ZIG_APP)
        return()
    endif()

    target_link_libraries(${TARGET} PRIVATE arcs-zig)
    set(ZIG_FORWARD_ARGS "")
    if(ZIG_TARGET_DEPENDS)
        list(APPEND ZIG_FORWARD_ARGS DEPENDS ${ZIG_TARGET_DEPENDS})
    endif()
    if(ZIG_TARGET_MODULES)
        list(APPEND ZIG_FORWARD_ARGS MODULES ${ZIG_TARGET_MODULES})
    endif()
    if(ZIG_TARGET_USE_REGISTERED_MODULES)
        list(APPEND ZIG_FORWARD_ARGS USE_REGISTERED_MODULES ${ZIG_TARGET_USE_REGISTERED_MODULES})
    endif()
    if(ZIG_TARGET_C_INCLUDE_DIRS)
        list(APPEND ZIG_FORWARD_ARGS C_INCLUDE_DIRS ${ZIG_TARGET_C_INCLUDE_DIRS})
    endif()
    zig_add_target_source(${TARGET} ${LISTENAI_ZIG_APP} ${ZIG_FORWARD_ARGS})
endfunction()

cmake_policy(POP)
