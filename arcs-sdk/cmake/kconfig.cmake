macro(import_kconfig prefix kconfig_fragment)
    cmake_parse_arguments(IMPORT_KCONFIG "" "TARGET" "" ${ARGN})
    file(
        STRINGS
        ${kconfig_fragment}
        DOT_CONFIG_LIST
        REGEX "^${prefix}"
        ENCODING "UTF-8"
    )

    foreach (CONFIG ${DOT_CONFIG_LIST})
        # maybe prefix is empty string
        if(CONFIG MATCHES "^#")
            continue()
        endif()

        # CONFIG could look like: CONFIG_NET_BUF=y
        # Match the first part, the variable name
        string(REGEX MATCH "[^=]+" CONF_VARIABLE_NAME ${CONFIG})

        # Match the second part, variable value
        string(REGEX MATCH "=(.+$)" CONF_VARIABLE_VALUE ${CONFIG})
        # The variable name match we just did included the '=' symbol. To just get the
        # part on the RHS we use match group 1
        set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})

        if("${CONF_VARIABLE_VALUE}" MATCHES "^\"(.*)\"$") # Is surrounded by quotes
            set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})
        endif()

        set(${CONF_VARIABLE_NAME} ${CONF_VARIABLE_VALUE})
    endforeach()
endmacro()

if (NOT DEFINED APPLICATION_SOURCE_DIR)
    set(APPLICATION_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH
            "Application Source Directory"
        )
endif()

option(LISTENAI_SDK_KCONFIG_PARSE "kconfig parse" ON)
set(LISTENAI_MODULES_KCONFIG_FILE ${CMAKE_BINARY_DIR}/module.Kconfig)

if (EXISTS ${LISTENAI_MODULES_KCONFIG_FILE})
    file(REMOVE ${LISTENAI_MODULES_KCONFIG_FILE})
endif()

get_property(LISTENAI_MODULES_PROPERTY GLOBAL PROPERTY LISTENAI_MODULES)
set(ENV{LISTENAI_MODULES} ${CMAKE_BINARY_DIR}/module.Kconfig)

file(APPEND ${LISTENAI_MODULES_KCONFIG_FILE} "menu \"modules\"\n")

file(APPEND ${LISTENAI_MODULES_KCONFIG_FILE} "osource \"${LISTENAI_CMAKE_PATH}/Kconfig\"\n")

foreach(module IN LISTS LISTENAI_MODULES_PROPERTY)
    list(FIND LISTENAI_KCONFIG_CUSTOM_PARSE_DIR_LIST ${module} idx)
    if (idx EQUAL -1)
        if (EXISTS ${module}/Kconfig)
            file(APPEND ${LISTENAI_MODULES_KCONFIG_FILE} "osource \"${module}/Kconfig\"\n")
        endif()
    endif()
endforeach()

file(APPEND ${LISTENAI_MODULES_KCONFIG_FILE} "endmenu\n")

macro(listenai_kconfig_parse kconfig_root dot_config autoconf_h kconfig_list conf_merge prefix)
    message(STATUS "Parse kconfig: ${${kconfig_root}}, prefix: ${${prefix}}")

    set(ENV{CONFIG_} ${${prefix}})
    execute_process(
        COMMAND
        ${LISTENAI_TOOLS_KCONFIG}
        -k ${${kconfig_root}}
        -c ${${dot_config}}
        -H ${${autoconf_h}}
        -l ${${kconfig_list}}
        -m ${${conf_merge}}
        WORKING_DIRECTORY ${APPLICATION_SOURCE_DIR}
        RESULT_VARIABLE result
    )

    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Kconfig parse failed, error code: ${result}")
    endif()

    import_kconfig("${${prefix}}" ${${dot_config}})
    add_compile_options(-imacros${${autoconf_h}})
endmacro()

if (LISTENAI_SDK_KCONFIG_PARSE)
    if (NOT EXISTS ${APPLICATION_SOURCE_DIR}/Kconfig)
        set(KCONFIG_ROOT ${LISTENAI_SDK_BASE}/Kconfig)
    else()
        set(KCONFIG_ROOT ${APPLICATION_SOURCE_DIR}/Kconfig)
    endif()

    if (NOT DEFINED DOT_CONFIG)
        set(DOT_CONFIG ${CMAKE_BINARY_DIR}/.config)
    endif()

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated/include)

    if (NOT DEFINED AUTOCONF_H)
        set(AUTOCONF_H ${CMAKE_BINARY_DIR}/generated/include/autoconf.h)
    endif()

    if (NOT DEFINED KCONFIG_MERGE_LIST)
        set(KCONFIG_MERGE_LIST "")
    endif()

    # 首先处理命令行CONFIG_*参数，生成extra_kconfig_options.conf
    unset(EXTRA_KCONFIG_OPTIONS)
    get_cmake_property(cache_variable_names CACHE_VARIABLES)
    foreach (name ${cache_variable_names})
      if("${name}" MATCHES "^CONFIG_")
        # When a cache variable starts with 'CONFIG_', it is assumed to be
        # a Kconfig symbol assignment from the CMake command line.
        set(EXTRA_KCONFIG_OPTIONS
          "${EXTRA_KCONFIG_OPTIONS}\n${name}=${${name}}"
          )
      endif()
    endforeach()

    if(EXTRA_KCONFIG_OPTIONS)
      set(EXTRA_KCONFIG_OPTIONS_FILE ${CMAKE_BINARY_DIR}/misc/generated/extra_kconfig_options.conf)
      # 确保目录存在
      file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/misc/generated)
      file(WRITE
        ${EXTRA_KCONFIG_OPTIONS_FILE}
        ${EXTRA_KCONFIG_OPTIONS}
        )
    endif()

    if (NOT DEFINED KCONFIG_LIST)
        set(KCONFIG_LIST ${CMAKE_BINARY_DIR}/kconfig.list)
    endif()

    if (EXISTS ${DOT_CONFIG})
        list(APPEND KCONFIG_MERGE_LIST ${DOT_CONFIG})
    endif()

    if (NOT DEFINED CONFIG_DEFAULT)
        set(CONFIG_DEFAULT ${APPLICATION_SOURCE_DIR}/prj.conf)
    endif()

    if (NOT IS_ABSOLUTE ${CONFIG_DEFAULT})
        set(CONFIG_DEFAULT ${APPLICATION_SOURCE_DIR}/${CONFIG_DEFAULT})
    endif()

    if (EXISTS ${CONFIG_DEFAULT})
        list(APPEND KCONFIG_MERGE_LIST ${CONFIG_DEFAULT})
    endif()

    if (EXISTS ${APPLICATION_SOURCE_DIR}/.config)
        list(APPEND KCONFIG_MERGE_LIST ${APPLICATION_SOURCE_DIR}/.config)
    endif()

    if (DEFINED CONFIG_FILES)
        foreach(config_file IN LISTS CONFIG_FILES)
            if (IS_ABSOLUTE ${config_file})
                list(APPEND KCONFIG_MERGE_LIST ${config_file})
            else()
                list(APPEND KCONFIG_MERGE_LIST ${APPLICATION_SOURCE_DIR}/${config_file})
            endif()
        endforeach()
    endif()

    # 添加命令行CONFIG_*参数文件到合并列表的最后（最高优先级）
    if(EXTRA_KCONFIG_OPTIONS_FILE AND EXISTS ${EXTRA_KCONFIG_OPTIONS_FILE})
        list(APPEND KCONFIG_MERGE_LIST ${EXTRA_KCONFIG_OPTIONS_FILE})
        message(STATUS "Added command-line CONFIG options file: ${EXTRA_KCONFIG_OPTIONS_FILE}")
    endif()

    string(JOIN " " KCONFIG_MERGE_LIST_STRING "${KCONFIG_MERGE_LIST}")

    message(STATUS "Kconfig merge list: ${KCONFIG_MERGE_LIST_STRING}")

    if (NOT DEFINED LISTENAI_KCONFIG_PREFIX)
        set(LISTENAI_KCONFIG_PREFIX "CONFIG_")
    endif()

    # parse main kconfig
    listenai_kconfig_parse(
        KCONFIG_ROOT
        DOT_CONFIG
        AUTOCONF_H
        KCONFIG_LIST
        KCONFIG_MERGE_LIST_STRING
        LISTENAI_KCONFIG_PREFIX
    )

    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/kconfig/custom)

    message(STATUS "Custom kconfig parse list: ${LISTENAI_KCONFIG_CUSTOM_PARSE_DIR_LIST}")

    list(LENGTH LISTENAI_KCONFIG_CUSTOM_PARSE_PREFIX_LIST prefix_list_len)

    foreach(item IN LISTS LISTENAI_KCONFIG_CUSTOM_PARSE_DIR_LIST)
        get_filename_component(ITEM_DIRECTORY_NAME ${item} NAME)
        string(TOLOWER ${ITEM_DIRECTORY_NAME} ITEM_DIRECTORY_NAME)

        set(ITEM_KCONFIG_ROOT ${item}/Kconfig)
        set(ITEM_DOTCONFIG ${CMAKE_BINARY_DIR}/kconfig/custom/${ITEM_DIRECTORY_NAME}.config)
        set(ITEM_AUTOCONF_H ${CMAKE_BINARY_DIR}/generated/include/${ITEM_DIRECTORY_NAME}_autoconf.h)
        set(ITEM_KCONFIG_LIST ${CMAKE_BINARY_DIR}/kconfig/custom/${ITEM_DIRECTORY_NAME}_kconfig.list)
        list(FIND LISTENAI_KCONFIG_CUSTOM_PARSE_DIR_LIST ${item} idx)

        if (${prefix_list_len} GREATER 0)
            if (${idx} LESS ${prefix_list_len})
                list(GET LISTENAI_KCONFIG_CUSTOM_PARSE_PREFIX_LIST ${idx} ITEM_KCONFIG_PREFIX)
            else()
                math(EXPR idx "${prefix_list_len} - 1")
                list(GET LISTENAI_KCONFIG_CUSTOM_PARSE_PREFIX_LIST ${idx} ITEM_KCONFIG_PREFIX)
            endif()
        else()
            set(ITEM_KCONFIG_PREFIX "CONFIG_")
        endif()

        if (${ITEM_KCONFIG_PREFIX} STREQUAL "\"\"")
            set(ITEM_KCONFIG_PREFIX "")
        endif()

        listenai_kconfig_parse(
            ITEM_KCONFIG_ROOT
            ITEM_DOTCONFIG
            ITEM_AUTOCONF_H
            ITEM_KCONFIG_LIST
            KCONFIG_MERGE_LIST_STRING
            ITEM_KCONFIG_PREFIX
        )
    endforeach()
endif()

add_custom_target(menuconfig
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/.config.old
    COMMAND ${CMAKE_COMMAND} -E env
    LISTENAI_MODULES=${CMAKE_BINARY_DIR}/module.Kconfig
    KCONFIG_CONFIG=${CMAKE_BINARY_DIR}/.config
    ${LISTENAI_TOOLS_MENUCONFIG}
    WORKING_DIRECTORY ${APPLICATION_SOURCE_DIR}
    COMMENT "running menuconfig"
    USES_TERMINAL
)



# 命令行CONFIG_*参数处理已移动到kconfig_setup函数内部，确保在合并列表构建时可用
