add_library(listenai_interface INTERFACE "")

macro(listenai_ex_sections _sections)
    set(LISTENAI_EX_SECTIONS ${_sections})
endmacro()

# 简介: 定义一个名称为`_name`的静态库
# 参数:
# + _name 指定该静态库的名称
macro(listenai_library_named _name)
    add_library(${_name} STATIC "")
    set(LISTENAI_CURRENT_LIBRARY ${_name})
    listenai_append_cmake_library(${_name})
    target_link_libraries(${_name} PUBLIC listenai_interface)
endmacro()

macro(listenai_psram_library_named _name)
    listenai_library_named(psram_${_name})
endmacro()

# 简介: 为当前的目标添加源文件
# 提示: 必须在使用`listenai_library_named`之后使用
# 参数:
# + source 源文件
# + ${ARGN} 可不參數
#
# 使用示例:
# ```
# listenai_library_sources(test.c test1.c test2.c test3.c)
# ```
function(listenai_library_sources source)
    target_sources(${LISTENAI_CURRENT_LIBRARY} PRIVATE ${source} ${ARGN})
endfunction()

# 简介: 当配置有效时,为当前的目标添加源文件
# 提示: 必须在使用`listenai_library_named`之后使用
# 参数:
# + feature_toggle 配置选项
# + ${ARGN} 可不參數
#
# 使用示例:
# 当`CONFIG_TEST`有效时,将添加源文件
# ```
# listenai_library_sources_ifdef(CONFIG_TEST test.c test1.c test2.c test3.c)
# ```
#
function(listenai_library_sources_ifdef feature_toggle)
    if(${${feature_toggle}})
        listenai_library_sources(${ARGN})
    endif()
endfunction()

# 简介: 为当前的目标提添加编译选项
# 提示: 必须在使用`listenai_library_named`之后使用
# 参数:
# + scope 作用范围, 可选择:PRIVATE, PUBLIC
# + option 编译选项
# + ${ARGN} 可不參數
function(listenai_library_compile_options scope option)
    target_compile_options(${LISTENAI_CURRENT_LIBRARY} ${scope} ${option} ${ARGN})
endfunction()

# 简介: 当配置项有效时, 为当前的目标提添加编译选项
# 提示: 必须在使用`listenai_library_named`之后使用
# 参数:
# + feature_toggle 配置项
# + scope 作用范围, 可选择:PRIVATE, PUBLIC
# + option 编译选项
# + ${ARGN} 可不參數
function(listenai_library_compile_options_ifdef  feature_toggle scope option)
    if(${${feature_toggle}})
        target_compile_options(${LISTENAI_CURRENT_LIBRARY} ${scope} ${option} ${ARGN})
    endif()
endfunction()

# 简介: 将指定的目标添加到`LISTENAI_LIBS`属性中
# 参数:
# + library 待添加的目标
function(listenai_append_cmake_library library)
    set_property(GLOBAL APPEND PROPERTY LISTENAI_LIBS ${library})
endfunction()

# 简介: 当配置有效时, 添加子目录
# 参数:
# + feature_toggle 配置项
# + dir 子目录
function(listenai_add_subdirectory_ifdef feature_toggle dir)
    if(${${feature_toggle}})
        add_subdirectory(${dir})
    endif()
endfunction()

# 简介: 当配置有效时, 为指定的目标添加源文件
# 参数:
# + feature_toggle 配置项
# + target 指定的目标
# + scope 作用范围,PRIVATE PUBLIC INTERFACE
# + item 源文件
# + ${ARGN} 可不參數
function(listenai_target_sources_ifdef feature_toggle target scope item)
    if(${${feature_toggle}})
        target_sources(${target} ${scope} ${item} ${ARGN})
    endif()
endfunction()

# 简介: 当配置有效时, 为指定的目标添添加宏定义
# 参数:
# + feature_toggle 配置项
# + target 指定的目标
# + scope 作用范围 PRIVATE, PUBLIC, INTERFACE
# + item 宏定义
# + ${ARGV} 可变参数
function(listenai_target_compile_definitions_ifdef feature_toggle target scope item)
    if(${${feature_toggle}})
        target_compile_definitions(${target} ${scope} ${item} ${ARGN})
    endif()
endfunction()

# 简介: 当配置有效时, 为指定的目标添加头文件路径
# 参数:
# + feature_toggle 配置项
# + target 指定的目标
# + scope 作用范围 PRIVATE, PUBLIC, INTERFACE
# + item 待被链接的目标
# + ${ARGV} 可变参数
function(listenai_target_include_directories_ifdef feature_toggle target scope item)
    if(${${feature_toggle}})
        target_include_directories(${target} ${scope} ${item} ${ARGN})
    endif()
endfunction()

# 简介: 当配置有效时, 为指定的目标连接其他目标
# 参数:
# + feature_toggle 配置项
# + target 指定的目标
# + item 待被链接的目标
# + ${ARGV} 可变参数
function(listenai_target_link_libraries_ifdef feature_toggle target item)
    if(${${feature_toggle}})
        target_link_libraries(${target} ${item} ${ARGN})
    endif()
endfunction()

# 简介: 当配置有效时, 添加全局的编译选项
# 参数:
# + feature_toggle 配置项
# + option 编译选项
# + ${ARGV} 可变参数
function(listenai_add_compile_option_ifdef feature_toggle option)
    if(${${feature_toggle}})
        add_compile_options(${option})
    endif()
endfunction()

# 简介: 当配置有效时, 为指定的目标添加编译选项
# 参数:
# + feature_toggle 配置项
# + target 指定的目标
# + scope 作用范围
# + option 编译选项
# + ${ARGV} 可变参数
function(listenai_target_compile_option_ifdef feature_toggle target scope option)
    if(${feature_toggle})
        target_compile_options(${target} ${scope} ${option} ${ARGV})
    endif()
endfunction()

# 简介: 为listenai_interface添加编译选项
# 参数:
# + ${ARGV} 可变参数
function(listenai_compile_options)
    target_compile_options(listenai_interface INTERFACE ${ARGV})
endfunction()

# 简介: 为listenai_interface添加宏定义
# 参数:
# + ${ARGV} 可变参数
function(listenai_compile_definitions)
    target_compile_definitions(listenai_interface INTERFACE ${ARGV})
endfunction()

# 简介: 当配置有效时, 为listenai_interface添加宏定义
# 参数:
# + feature_toggle 配置项
# + ${ARGV} 可变参数
function(listenai_compile_definitions_ifdef feature_toggle)
    if(${${feature_toggle}})
        listenai_compile_definitions(${ARGN})
    endif()
endfunction()

# 简介: 将目标链接到listenai_interface中
# 参数:
# + item 被连接的目标
function(listenai_link_libraries item)
    target_link_libraries(listenai_interface INTERFACE ${item} ${ARGV})
    foreach(arg ${ARGV})
        set_property(GLOBAL APPEND PROPERTY LISTENAI_LIBS ${arg})
    endforeach()
endfunction()

# 简介: 将头文件加入listenai_interface中
# 参数:
# + target_name 目标
function(listenai_include_directories)
    foreach(arg ${ARGV})
        if(IS_ABSOLUTE ${arg})
            set(path ${arg})
        else()
            set(path ${CMAKE_CURRENT_SOURCE_DIR}/${arg})
        endif()
        target_include_directories(listenai_interface INTERFACE ${path})
    endforeach()
endfunction()

# 简介: 为目标生成bin文件
# 参数:
# + target_name 目标
macro(listenai_generate_bin target_name)
    if(LISTENAI_EX_SECTIONS)
        # 如果定义了 LISTENAI_EX_SECTIONS，分别生成两个bin文件
        add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.bin"
            COMMAND ${CMAKE_OBJCOPY} -S -R ${LISTENAI_EX_SECTIONS} -O binary ${target_name} ${target_name}.bin
            COMMAND ${CMAKE_OBJCOPY} -S -j ${LISTENAI_EX_SECTIONS} -O binary ${target_name} ${target_name}_flash2.bin
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        # 如果没有定义 LISTENAI_EX_SECTIONS，只生成一个bin文件
        add_custom_command(
            TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.bin"
            COMMAND ${CMAKE_OBJCOPY} -S -O binary ${target_name} ${target_name}.bin
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
endmacro()

# 简介: 为目标生成hex文件
# 参数:
# + target_name 目标
macro(listenai_generate_hex target_name)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.hex"
        COMMAND ${CMAKE_OBJCOPY} -S -O ihex
        ${target_name}
        ${target_name}.hex
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endmacro()

# 简介: 为目标生成调试信息，包括反汇编、ELF头、符号表等
# 参数:
# + target_name 目标
#
# 注意（macOS）：binutils 2.44 的 `objdump -d -S` 对中等规模 ELF 存在
# O(N²)~O(N³) 的算法退化（每 4× 代码量时间膨胀 20~300 倍）。这个问题在
# Linux 上因 per-op 常量低（malloc / vnode cache / syscall path）只表现为
# 几秒到几分钟；在 Apple Silicon macOS 上 per-op 开销大 ~10×，会让
# ninja "Linking C executable ..." 卡几十分钟甚至小时级（cmake 把
# POST_BUILD 合并到 link step）。APPLE 分支下仅生成纯反汇编（不带 -S），
# Linux 行为不变。需要源码交织时可手动对小范围跑：
#     objdump -d -S --start-address=X --stop-address=Y <elf>
if(APPLE)
    set(_LISTENAI_OBJDUMP_DISASM_FLAGS -d)
else()
    set(_LISTENAI_OBJDUMP_DISASM_FLAGS -d -S)
endif()

macro(listenai_generate_debug_files target_name)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.lst"
        COMMAND ${CMAKE_OBJDUMP} ${_LISTENAI_OBJDUMP_DISASM_FLAGS} ${target_name} > ${target_name}.lst
        COMMAND ${CMAKE_READELF} -a ${target_name} > ${target_name}.relf
        COMMAND ${CMAKE_NM} -CSsnl -f sysv ${target_name} > ${target_name}.symb
        COMMAND ${CMAKE_SIZE} -B ${target_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endmacro()

# 简介: 将所有通过listenai cmake扩展定义的目标连接到指定的目标中
# 参数:
# + target_name 将要链接到的目标
macro(listenai_link_all_modules target_name)
    get_property(LISTENAI_LIBS_PROPERTY GLOBAL PROPERTY LISTENAI_LIBS)
    list(REMOVE_DUPLICATES LISTENAI_LIBS_PROPERTY)

    # 获取目标已手动链接的库，避免重复链接
    get_target_property(_existing_libs ${target_name} LINK_LIBRARIES)
    if(_existing_libs)
        list(REMOVE_ITEM LISTENAI_LIBS_PROPERTY ${_existing_libs})
    endif()

    if (LISTENAI_LIBS_PROPERTY)
        # 硬编码排除libm.a以避免符号冲突
        # 2025.02版本工具链lib.m存在两个frexpl实现，经芯来原厂沟通，两个实现都可用
        set(EXCLUDE_LIB_NAME "m")
        
        set(WHOLE_ARCHIVE_LIBS)
        set(NORMAL_LIBS)
        
        # 分离需要全量链接的库和普通库
        foreach(lib ${LISTENAI_LIBS_PROPERTY})
            set(is_excluded FALSE)
            # 检查库名是否匹配排除的库
            if(lib MATCHES "lib${EXCLUDE_LIB_NAME}\\.(a|so)$" OR lib STREQUAL "${EXCLUDE_LIB_NAME}")
                set(is_excluded TRUE)
            endif()
            
            if(is_excluded)
                list(APPEND NORMAL_LIBS ${lib})
            else()
                list(APPEND WHOLE_ARCHIVE_LIBS ${lib})
            endif()
        endforeach()

        set(LINK_START_CMD)
        set(LINK_END_CMD)

        if (DEFINED CONFIG_LINK_OPTION_LISTENAI_LIBRARY_GROUP)
            list(APPEND LINK_START_CMD "-Wl,--start-group")
            list(APPEND LINK_END_CMD "-Wl,--end-group")
        endif()

        # 根据whole-archive配置选择链接方式
        if (DEFINED CONFIG_LINK_OPTION_LISTENAI_LIBRARY_WHOLE_ARCHIVE)
            # 开启了whole-archive，分别链接全量库和普通库
            if (WHOLE_ARCHIVE_LIBS)
                target_link_libraries(${target_name} PRIVATE 
                    ${LINK_START_CMD}
                    "-Wl,--whole-archive" 
                    ${WHOLE_ARCHIVE_LIBS} 
                    "-Wl,--no-whole-archive"
                    ${LINK_END_CMD})
            endif()
            
            if(NORMAL_LIBS)
                target_link_libraries(${target_name} PRIVATE ${NORMAL_LIBS})
            endif()
        else()
            # 没有开启whole-archive，使用原来的逻辑
            target_link_libraries(${target_name} PRIVATE ${LINK_START_CMD} ${LISTENAI_LIBS_PROPERTY} ${LINK_END_CMD})
        endif()
    endif()
endmacro()

# 添加可执行文件
# 此宏会自动为可执行文件添加bin,hex,lst文件输出
# 此宏会自动扫描LISTENAI_MODULES_DIR_LIST 列表中的目录, 并添加模块到构建系统中
macro(listenai_add_executable name)
    set(LISTENAI_EXECUTABLE_NAME ${name})
    add_executable(${name})
    target_sources(${name} PRIVATE ${LISTENAI_CMAKE_PATH}/empty.c)
    listenai_generate_bin(${name})
    listenai_generate_hex(${name})
    if(CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES)
        listenai_generate_debug_files(${name})
    endif()
    if(LISTENAI_ADD_BIN_HEADR)
        set(_boot_hdr_args "")
        if(LISTENAI_MKHDR_TARGET_CORE)
            list(APPEND _boot_hdr_args TARGET_CORE)
        endif()
        listenai_generate_boot_header(${name} ${_boot_hdr_args})
    endif()

    # 立即扫描并添加模块，但延迟链接操作
    # 这样可以确保所有 add_subdirectory 执行完后再链接，不受调用顺序影响
    get_property(LISTENAI_MODULES_PROPERTY GLOBAL PROPERTY LISTENAI_MODULES)
    foreach(module IN LISTS LISTENAI_MODULES_PROPERTY)
        message(STATUS "Found module: ${module} ")
        if(CMAKE_HOST_WIN32)
            get_filename_component(_module_name "${module}" NAME)
            string(MD5 _module_hash "${module}")
            set(_module_binary_dir "${CMAKE_BINARY_DIR}/modules/${_module_name}-${_module_hash}")
        else()
            set(_module_binary_dir "${CMAKE_BINARY_DIR}/modules/${module}")
        endif()
        add_subdirectory(${module} ${_module_binary_dir})
    endforeach()

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
        # 延迟链接到配置阶段末尾，确保用户的 add_subdirectory 也执行完成
        cmake_language(DEFER CALL listenai_link_all_modules ${name})
    else()
        # CMake 版本过低，无法使用 DEFER 机制
        message(FATAL_ERROR "CMake 3.19 or higher is required, but current version is ${CMAKE_VERSION}")
    endif()

    set(LISTENAI_CURRENT_LIBRARY "_inner_app")
    set(LISTENAI_EXECUTABLE_COMPLETED TRUE)
endmacro()

macro(listenai_add_boot_only name)
    include(ExternalProject)

    if(NOT DEFINED BOOT_STANDALONE_UBOOT_ROOT)
        set(BOOT_STANDALONE_UBOOT_ROOT "${ARCS_SDK_BASE}/system/uboot")
    endif()

    if(NOT DEFINED BOOT_STANDALONE_CONFIG_DEFAULT)
        set(BOOT_STANDALONE_CONFIG_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf")
    endif()

    if(IS_ABSOLUTE "${BOOT_STANDALONE_CONFIG_DEFAULT}")
        set(_boot_only_config_path "${BOOT_STANDALONE_CONFIG_DEFAULT}")
    else()
        get_filename_component(
            _boot_only_config_path
            "${BOOT_STANDALONE_CONFIG_DEFAULT}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()

    set(_boot_only_binary_dir "${CMAKE_BINARY_DIR}/boot")
    set(_boot_only_output "${_boot_only_binary_dir}/${name}.bin")
    set(_boot_only_app_config_file "${_boot_only_binary_dir}/app.config")
    set(_boot_only_main_dot_config "${CMAKE_BINARY_DIR}/.config")
    set(_boot_only_configs)
    set(_boot_only_extra_args)

    file(MAKE_DIRECTORY "${_boot_only_binary_dir}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_boot_only_config_path}"
        "${_boot_only_main_dot_config}"
    )

    if(EXISTS "${_boot_only_main_dot_config}")
        file(
            STRINGS "${_boot_only_main_dot_config}" _boot_only_config_lines
            REGEX "^(CONFIG_BOOT_|# CONFIG_BOOT_|CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES=|# CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES is not set)"
            ENCODING "UTF-8"
        )

        foreach(_boot_only_line IN LISTS _boot_only_config_lines)
            if(_boot_only_line MATCHES "^CONFIG_BOOT_HART=" OR
               _boot_only_line MATCHES "^# CONFIG_BOOT_HART is not set")
                continue()
            endif()
            list(APPEND _boot_only_configs "${_boot_only_line}")
        endforeach()
    else()
        get_cmake_property(_boot_only_vars VARIABLES)
        list(SORT _boot_only_vars)
        foreach(_boot_only_var IN LISTS _boot_only_vars)
            if(_boot_only_var MATCHES "^CONFIG_BOOT_" AND
               NOT _boot_only_var STREQUAL "CONFIG_BOOT_HART")
                list(APPEND _boot_only_configs "${_boot_only_var}=${${_boot_only_var}}")
            endif()
        endforeach()

        if(DEFINED CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES)
            if(CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES)
                list(APPEND _boot_only_configs "CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES=y")
            else()
                list(APPEND _boot_only_configs "# CONFIG_COMPILE_OPTION_GENERATE_DEBUG_FILES is not set")
            endif()
        endif()
    endif()

    file(WRITE "${_boot_only_app_config_file}" "")
    foreach(_boot_only_config IN LISTS _boot_only_configs)
        file(APPEND "${_boot_only_app_config_file}" "${_boot_only_config}\n")
    endforeach()

    set(_boot_only_fingerprint_input "")
    if(EXISTS "${_boot_only_config_path}")
        file(SHA256 "${_boot_only_config_path}" _boot_only_default_config_hash)
        string(APPEND _boot_only_fingerprint_input "${_boot_only_default_config_hash}")
    endif()
    file(SHA256 "${_boot_only_app_config_file}" _boot_only_app_config_hash)
    string(APPEND _boot_only_fingerprint_input ":${_boot_only_app_config_hash}")

    set(_boot_only_config_files "${_boot_only_app_config_file}")
    set(_boot_only_board_config "${CMAKE_CURRENT_SOURCE_DIR}/${BOARD}.conf")
    if(EXISTS "${_boot_only_board_config}")
        list(APPEND _boot_only_config_files "${_boot_only_board_config}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${_boot_only_board_config}"
        )
        file(SHA256 "${_boot_only_board_config}" _boot_only_board_config_hash)
        string(APPEND _boot_only_fingerprint_input ":${_boot_only_board_config_hash}")
    endif()

    if(_boot_only_fingerprint_input)
        string(SHA256 _boot_only_config_fingerprint "${_boot_only_fingerprint_input}")
        list(APPEND _boot_only_extra_args
            -DBOOT_STANDALONE_CONFIG_FINGERPRINT=${_boot_only_config_fingerprint}
        )
    endif()

    string(REPLACE ";" "|" _boot_only_config_files_arg "${_boot_only_config_files}")

    set(LISTENAI_MKHDR_TARGET_CORE FALSE)
    if(EXISTS "${_boot_only_config_path}")
        file(STRINGS "${_boot_only_config_path}" _boot_only_prj_lines)
        foreach(_line IN LISTS _boot_only_prj_lines)
            if(_line MATCHES "^CONFIG_BOOT_APP_CORE_AUTO=y")
                set(LISTENAI_MKHDR_TARGET_CORE TRUE)
            endif()
        endforeach()
    endif()

    ExternalProject_Add(
        boot_external
        SOURCE_DIR ${BOOT_STANDALONE_UBOOT_ROOT}
        BINARY_DIR ${_boot_only_binary_dir}
        LIST_SEPARATOR "|"
        CMAKE_ARGS
            -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DCHIP=${CHIP}
            -DBOARD=${BOARD}
            -DARCS_SDK_BASE=${ARCS_SDK_BASE}
            -DLISTENAI_TOOLS_PATH=${LISTENAI_TOOLS_PATH}
            -DENABLE_DEBUG_PATH=${ENABLE_DEBUG_PATH}
            -DBOOT_STANDALONE_PROJECT_NAME=${name}
            -DBOOT_STANDALONE_CONFIG_DEFAULT=${_boot_only_config_path}
            -DCONFIG_FILES=${_boot_only_config_files_arg}
            ${_boot_only_extra_args}
        BUILD_COMMAND ${CMAKE_COMMAND} --build .
        INSTALL_COMMAND ""
    )

    add_custom_target(
        boot_only_artifact
        ALL
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating boot.bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_boot_only_output}
            ${CMAKE_BINARY_DIR}/boot.bin
        DEPENDS boot_external
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endmacro()

# 为可执行文件添加listenai boot header
macro(listenai_generate_boot_header target_name)
    cmake_parse_arguments(_BOOT_HDR "TARGET_CORE" "" "" ${ARGN})
    set(_mkhdr_flags_arg "")
    if(NOT DEFINED LISTENAI_TOOLS_MKHDR_COMMAND)
        set(LISTENAI_TOOLS_MKHDR_COMMAND "${LISTENAI_TOOLS_MKHDR}")
    endif()
    if(_BOOT_HDR_TARGET_CORE AND DEFINED CONFIG_HARTID)
        execute_process(
            COMMAND ${LISTENAI_TOOLS_MKHDR_COMMAND} -h
            OUTPUT_VARIABLE _mkhdr_help
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_mkhdr_help MATCHES "-f ")
            set(_mkhdr_flags_arg -f ${CONFIG_HARTID})
        else()
            message(WARNING "mkhdr does not support -f (target_core); update mkhdr to write boot core into image header")
        endif()
    endif()
    add_custom_target(
        mkhdr ALL
        COMMAND ${CMAKE_COMMAND} -E echo "-- Generating ListenAI Boot Header for ${target_name}.bin"
        COMMAND ${LISTENAI_TOOLS_MKHDR_COMMAND} ${_mkhdr_flags_arg} ${target_name}.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_dependencies(mkhdr ${target_name})
endmacro()

# ---------------------------------------------------------------------------
# Linker script fragment injection (SLOT-based, inspired by Zephyr)
# ---------------------------------------------------------------------------
# Components register MEMORY regions, SECTION fragments, and scatter table
# entries via these macros. At configure time, sorted snippets-*.ld files
# are generated containing #include directives. system.ld #includes these
# at predefined slot positions.
#
# Available SLOTs:
#   MEMORY          - inside MEMORY{} block
#   ALIASES         - REGION_ALIAS declarations after MEMORY{}
#   SECTIONS_START  - top of SECTIONS{}, for early NOLOAD (e.g. WiFi RAM)
#   COMPONENTS      - after PSRAM core sections, for component section groups
#   ROM             - before .text, for component ROM sections
#   RAM             - before .data, for component RAM AT>ROM sections
#   POST_DATA       - after .data, for late NOLOAD (e.g. IPC)
#   SECTIONS_END    - end of SECTIONS{}, for tail sections (e.g. bt_heap)
#
# SORT_KEY 层级命名约定（字典序排列，确保多源注入的可预测排序）:
#   "10-soc"       - SoC 层片段 (soc/${CHIP}/linker/)
#   "20-board"     - Board 层片段 (boards/${BOARD}/linker/)
#   "30-component" - SDK 组件片段 (components/)
#   "50-app"       - 应用层片段
#   "default"      - 未指定时的默认值
# ---------------------------------------------------------------------------

# Register a MEMORY region fragment.
# Usage:
#   listenai_add_linker_memory(FILE <path> [SORT_KEY <key>])
macro(listenai_add_linker_memory)
    cmake_parse_arguments(_LM "" "FILE;SORT_KEY" "" ${ARGN})
    if(NOT _LM_FILE)
        message(FATAL_ERROR "listenai_add_linker_memory: FILE is required")
    endif()
    if(NOT _LM_SORT_KEY)
        set(_LM_SORT_KEY "default")
    endif()
    set_property(GLOBAL APPEND PROPERTY LISTENAI_SNIPPETS_memory "${_LM_SORT_KEY}|${_LM_FILE}")
endmacro()

# Register a SECTION fragment at a named SLOT position.
# Usage:
#   listenai_add_linker_section(FILE <path> SLOT <slot> [SORT_KEY <key>])
macro(listenai_add_linker_section)
    cmake_parse_arguments(_LS "" "FILE;SLOT;SORT_KEY" "" ${ARGN})
    if(NOT _LS_FILE)
        message(FATAL_ERROR "listenai_add_linker_section: FILE is required")
    endif()
    if(NOT _LS_SLOT)
        message(FATAL_ERROR "listenai_add_linker_section: SLOT is required")
    endif()
    if(NOT _LS_SORT_KEY)
        set(_LS_SORT_KEY "default")
    endif()
    string(TOLOWER "${_LS_SLOT}" _slot_lower)
    set_property(GLOBAL APPEND PROPERTY LISTENAI_SNIPPETS_${_slot_lower} "${_LS_SORT_KEY}|${_LS_FILE}")
endmacro()

# Register scatter table entries.
# Usage:
#   listenai_add_linker_scatter(SCATLOAD <section_name>)
#   listenai_add_linker_scatter(SCATZERO <section_name>)
macro(listenai_add_linker_scatter)
    cmake_parse_arguments(_LSC "" "SCATLOAD;SCATZERO" "" ${ARGN})
    if(_LSC_SCATLOAD)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATTER_LOAD "${_LSC_SCATLOAD}")
    endif()
    if(_LSC_SCATZERO)
        set_property(GLOBAL APPEND PROPERTY LISTENAI_SCATTER_ZERO "${_LSC_SCATZERO}")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# Code relocation API
# ---------------------------------------------------------------------------
# Relocate library, file, or named sections to a target memory region.
# Usage:
#   listenai_code_relocate(LIBRARY <lib_name> LOCATION <location> [EXCLUDE_OBJECTS <obj1> ...])
#   listenai_code_relocate(FILES <file1> [file2 ...] LOCATION <location>)
#   listenai_code_relocate(SECTIONS <sec1> [sec2 ...] LOCATION <location>)
#
# LIBRARY:  library name as passed to listenai_library_named() (without lib prefix)
# FILES:    source file names (basename only, e.g. "dsp_core.c")
# SECTIONS: input section names (e.g. ".pm.ramcode", "._wf_critical")
#           Each name auto-expands to: <name> <name>.*
#           SECTIONS mode requires a granular LOCATION (not aggregate).
# LOCATION: one of PSRAM, PSRAM_TEXT, PSRAM_RODATA, PSRAM_DATA, PSRAM_BSS,
#           SRAM, SRAM_TEXT, SRAM_RODATA, SRAM_DATA, SRAM_BSS,
#           ITCM, DTCM
#
# Aggregate locations expand:
#   PSRAM -> PSRAM_TEXT + PSRAM_RODATA + PSRAM_DATA + PSRAM_BSS
#   SRAM  -> SRAM_TEXT + SRAM_RODATA + SRAM_DATA + SRAM_BSS
#   DTCM  -> DTCM_DATA + DTCM_BSS  (mapped to .dtcm + .dtcm.bss)
# ---------------------------------------------------------------------------
function(listenai_code_relocate)
    cmake_parse_arguments(_CR "" "LIBRARY;LOCATION" "FILES;SECTIONS;EXCLUDE_OBJECTS" ${ARGN})

    if(NOT _CR_LOCATION)
        message(FATAL_ERROR "listenai_code_relocate: LOCATION is required")
    endif()

    # --- SECTIONS mode: collect named sections into a target ---
    if(_CR_SECTIONS)
        if(_CR_LIBRARY OR _CR_FILES OR _CR_EXCLUDE_OBJECTS)
            message(FATAL_ERROR "listenai_code_relocate: SECTIONS cannot be combined with LIBRARY, FILES, or EXCLUDE_OBJECTS")
        endif()
        # Map LOCATION to the single relocate target property
        string(TOLOWER "${_CR_LOCATION}" _loc_lower)
        set(_target "")
        foreach(_candidate
            sram_text sram_rodata sram_data sram_bss
            itcm dtcm_data dtcm_bss
            psram_text psram_rodata psram_data psram_bss)
            if(_loc_lower STREQUAL "${_candidate}")
                set(_target "${_candidate}")
                break()
            endif()
        endforeach()
        if(NOT _target)
            message(FATAL_ERROR "listenai_code_relocate: SECTIONS mode requires a granular LOCATION, got '${_CR_LOCATION}'")
        endif()
        # Build entry: * (<sec1> <sec1>.* <sec2> <sec2>.* ...)
        set(_patterns "")
        foreach(_sec ${_CR_SECTIONS})
            string(APPEND _patterns "${_sec} ${_sec}.* ")
        endforeach()
        string(STRIP "${_patterns}" _patterns)
        set_property(GLOBAL APPEND PROPERTY
            LISTENAI_RELOCATE_${_target} "* (${_patterns})")
        return()
    endif()

    if(NOT _CR_LIBRARY AND NOT _CR_FILES)
        message(FATAL_ERROR "listenai_code_relocate: LIBRARY, FILES, or SECTIONS is required")
    endif()
    if(_CR_LIBRARY AND _CR_FILES)
        message(FATAL_ERROR "listenai_code_relocate: LIBRARY and FILES are mutually exclusive")
    endif()
    if(_CR_EXCLUDE_OBJECTS AND NOT _CR_LIBRARY)
        message(FATAL_ERROR "listenai_code_relocate: EXCLUDE_OBJECTS requires LIBRARY mode")
    endif()

    # --- Expand aggregate locations to granular targets ---
    set(_targets)
    if(_CR_LOCATION STREQUAL "PSRAM")
        list(APPEND _targets psram_text psram_rodata psram_data psram_bss)
    elseif(_CR_LOCATION STREQUAL "SRAM")
        list(APPEND _targets sram_text sram_rodata sram_data sram_bss)
    elseif(_CR_LOCATION STREQUAL "DTCM")
        list(APPEND _targets dtcm_data dtcm_bss)
    elseif(_CR_LOCATION STREQUAL "PSRAM_TEXT")
        list(APPEND _targets psram_text)
    elseif(_CR_LOCATION STREQUAL "PSRAM_RODATA")
        list(APPEND _targets psram_rodata)
    elseif(_CR_LOCATION STREQUAL "PSRAM_DATA")
        list(APPEND _targets psram_data)
    elseif(_CR_LOCATION STREQUAL "PSRAM_BSS")
        list(APPEND _targets psram_bss)
    elseif(_CR_LOCATION STREQUAL "SRAM_TEXT")
        list(APPEND _targets sram_text)
    elseif(_CR_LOCATION STREQUAL "SRAM_RODATA")
        list(APPEND _targets sram_rodata)
    elseif(_CR_LOCATION STREQUAL "SRAM_DATA")
        list(APPEND _targets sram_data)
    elseif(_CR_LOCATION STREQUAL "SRAM_BSS")
        list(APPEND _targets sram_bss)
    elseif(_CR_LOCATION STREQUAL "ITCM")
        list(APPEND _targets itcm)
    else()
        message(FATAL_ERROR "listenai_code_relocate: unknown LOCATION '${_CR_LOCATION}'")
    endif()

    # --- Build the wildcard prefix (archive or object file) ---
    if(_CR_LIBRARY)
        set(_archive_prefix "*lib${_CR_LIBRARY}.a:")
        set(_exclude_patterns "")
        foreach(_member ${_CR_EXCLUDE_OBJECTS})
            list(APPEND _exclude_patterns "${_archive_prefix}${_member}")
        endforeach()
        string(REPLACE ";" " " _exclude_members "${_exclude_patterns}")
    endif()

    # --- Map each target to its input section patterns and register ---
    foreach(_target ${_targets})
        # Determine input section patterns for this target
        if(_target MATCHES "text$" OR _target STREQUAL "itcm")
            set(_section_patterns ".text .text.* .stext .stext.*")
        elseif(_target MATCHES "rodata$")
            set(_section_patterns ".rodata .rodata.* .srodata .srodata.*")
        elseif(_target MATCHES "_data$" OR _target STREQUAL "dtcm_data")
            set(_section_patterns ".data .data.* .sdata .sdata.*")
        elseif(_target MATCHES "bss$")
            set(_section_patterns ".bss .bss.* .sbss .sbss.* COMMON")
        else()
            message(FATAL_ERROR "listenai_code_relocate: internal error - unknown target '${_target}'")
        endif()

        if(_CR_LIBRARY)
            if(_exclude_members)
                set(_entry "EXCLUDE_FILE (${_exclude_members}) ${_archive_prefix}*(${_section_patterns})")
            else()
                set(_entry "${_archive_prefix}*(${_section_patterns})")
            endif()
            set_property(GLOBAL APPEND PROPERTY
                LISTENAI_RELOCATE_${_target} "${_entry}")
        else()
            # File-level: one entry per file
            foreach(_file ${_CR_FILES})
                get_filename_component(_basename "${_file}" NAME)
                # Match both .c.obj and .c.o patterns
                set_property(GLOBAL APPEND PROPERTY
                    LISTENAI_RELOCATE_${_target}
                    "*${_basename}*(${_section_patterns})")
            endforeach()
        endif()
    endforeach()
endfunction()

# Generate all snippets-*.ld files from collected fragments.
# Each snippet file contains sorted #include directives so that
# error messages reference original source files and line numbers.
# Called internally by listenai_set_linker_script().
function(listenai_generate_linker_snippets)
    set(_gen_dir "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_gen_dir}")

    # All slot names (property suffix → output filename)
    set(_slots
        memory aliases
        sections_start components rom post_text ram post_data sections_end
    )

    foreach(_slot ${_slots})
        get_property(_entries GLOBAL PROPERTY LISTENAI_SNIPPETS_${_slot})
        set(_output "${_gen_dir}/snippets-${_slot}.ld")
        if(_entries)
            # Sort entries by SORT_KEY (format: "key|filepath")
            list(SORT _entries)
            set(_content "/* Auto-generated snippets for: ${_slot} */\n")
            foreach(_entry ${_entries})
                string(REPLACE "|" ";" _parts "${_entry}")
                list(GET _parts 0 _key)
                list(GET _parts 1 _file)
                string(APPEND _content "/* Sort key: \"${_key}\" */#include \"${_file}\"\n")
            endforeach()
            file(WRITE "${_output}" "${_content}")
        else()
            file(WRITE "${_output}" "/* No fragments for: ${_slot} */\n")
        endif()
    endforeach()

    # Code relocation snippet files
    set(_relocate_targets
        psram_text psram_rodata psram_data psram_bss
        sram_text sram_rodata sram_data sram_bss
        itcm dtcm_data dtcm_bss
    )
    foreach(_target ${_relocate_targets})
        get_property(_entries GLOBAL PROPERTY LISTENAI_RELOCATE_${_target})
        string(REPLACE "_" "-" _fname "${_target}")
        set(_output "${_gen_dir}/snippets-relocate-${_fname}.ld")
        if(_entries)
            set(_content "/* Auto-generated code relocation entries for: ${_target} */\n")
            foreach(_entry ${_entries})
                string(APPEND _content "        ${_entry}\n")
            endforeach()
            file(WRITE "${_output}" "${_content}")
        else()
            file(WRITE "${_output}" "/* No relocations for: ${_target} */\n")
        endif()
    endforeach()

    # Scatter copy table entries
    get_property(_scatload GLOBAL PROPERTY LISTENAI_SCATTER_LOAD)
    set(_content "/* Auto-generated scatter copy entries */\n")
    foreach(_s ${_scatload})
        string(APPEND _content "            SCATLOAD(${_s})\n")
    endforeach()
    file(WRITE "${_gen_dir}/snippets-scatload.ld" "${_content}")

    # Scatter zero table entries
    get_property(_scatzero GLOBAL PROPERTY LISTENAI_SCATTER_ZERO)
    set(_content "/* Auto-generated scatter zero entries */\n")
    foreach(_s ${_scatzero})
        string(APPEND _content "            SCATZERO(${_s})\n")
    endforeach()
    file(WRITE "${_gen_dir}/snippets-scatzero.ld" "${_content}")
endfunction()

macro(listenai_set_linker_script linker_script)
    # Defer snippet generation until all add_subdirectory() calls complete,
    # so that components/drivers can register their linker fragments
    cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
        CALL listenai_generate_linker_snippets)

    get_property(LISTENAI_LINK_SCRIPTS_PROPERTY GLOBAL PROPERTY LISTENAI_LINK_SCRIPTS)
    get_property(INCLUDE_DIRS TARGET listenai_interface PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    get_property(LISTENAI_IF_DEFINITIONS TARGET listenai_interface PROPERTY INTERFACE_COMPILE_DEFINITIONS)

    set(INCLUDE_FLAGS "")
    foreach(dir ${INCLUDE_DIRS})
        list(APPEND INCLUDE_FLAGS "-I${dir}")
    endforeach()
    # Add build directory so #include "generated/sections_*.ld" resolves
    list(APPEND INCLUDE_FLAGS "-I${CMAKE_BINARY_DIR}")

    set(LISTENAI_IF_DEFINITIONS_FLAGS "")
    foreach(flag ${LISTENAI_IF_DEFINITIONS})
        list(APPEND LISTENAI_IF_DEFINITIONS_FLAGS "-D${flag}")
    endforeach()

    if(LISTENAI_LINK_SCRIPTS_PROPERTY)
        add_custom_target(linker_script_prepare
            COMMAND cat ${linker_script} ${LISTENAI_LINK_SCRIPTS_PROPERTY}  > ${CMAKE_BINARY_DIR}/linker.ld.pre
            DEPENDS ${linker_script} ${LISTENAI_LINK_SCRIPTS_PROPERTY}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        add_custom_target(linker_script_prepare
            COMMAND ${CMAKE_COMMAND} -E copy ${linker_script} ${CMAKE_BINARY_DIR}/linker.ld.pre
            DEPENDS ${linker_script}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()

    add_custom_target(generate_linker_script
        COMMAND ${CMAKE_C_COMPILER}
            -E
            -P
            -x assembler-with-cpp
            ${INCLUDE_FLAGS}
            ${LISTENAI_IF_DEFINITIONS_FLAGS}
            -include autoconf.h
            ${CMAKE_BINARY_DIR}/linker.ld.pre
            -o ${CMAKE_BINARY_DIR}/linker.ld
        DEPENDS linker_script_prepare
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Verify no MEMORY region overlaps in the generated linker script
    get_property(_overlap_wl GLOBAL PROPERTY LISTENAI_MEMORY_OVERLAP_WHITELIST)
    set(_overlap_wl_flag "")
    if(_overlap_wl)
        string(REPLACE ";" "\\;" _overlap_wl_escaped "${_overlap_wl}")
        set(_overlap_wl_flag "-DOVERLAP_WHITELIST=${_overlap_wl_escaped}")
    endif()
    add_custom_target(check_linker_memory_overlap
        COMMAND ${CMAKE_COMMAND}
            -DLINKER_SCRIPT=${CMAKE_BINARY_DIR}/linker.ld
            ${_overlap_wl_flag}
            -P ${ARCS_SDK_BASE}/cmake/check_memory_overlap.cmake
        DEPENDS generate_linker_script
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    add_dependencies(${LISTENAI_EXECUTABLE_NAME} check_linker_memory_overlap)
    target_link_options(${LISTENAI_EXECUTABLE_NAME} PRIVATE "-T${CMAKE_BINARY_DIR}/linker.ld")
endmacro()

macro(listenai_append_linker_script linker_script)
    set_property(GLOBAL APPEND PROPERTY LISTENAI_LINK_SCRIPTS ${linker_script})
endmacro()
