add_library(listenai_interface INTERFACE "")

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
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.bin"
        COMMAND ${CMAKE_OBJCOPY} -S -O binary ${target_name} ${target_name}.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
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
macro(listenai_generate_debug_files target_name)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating file: ${target_name}.lst"
        COMMAND ${CMAKE_OBJDUMP} -d -S ${target_name} > ${target_name}.lst
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
        listenai_generate_boot_header(${name})
    endif()

    # 立即扫描并添加模块，但延迟链接操作
    # 这样可以确保所有 add_subdirectory 执行完后再链接，不受调用顺序影响
    get_property(LISTENAI_MODULES_PROPERTY GLOBAL PROPERTY LISTENAI_MODULES)
    foreach(module IN LISTS LISTENAI_MODULES_PROPERTY)
        message(STATUS "Found module: ${module} ")
        add_subdirectory(${module} ${CMAKE_BINARY_DIR}/modules/${module})
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

# 为可执行文件添加listenai boot header
macro(listenai_generate_boot_header target_name)
    add_custom_target(
        mkhdr ALL
        COMMAND ${CMAKE_COMMAND} -E echo "-- Genarating ListenAI Boot Header for ${target_name}.bin"
        COMMAND ${LISTENAI_TOOLS_MKHDR} ${target_name}.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_dependencies(mkhdr ${target_name})
endmacro()

macro(listenai_set_linker_script linker_script)
    get_property(LISTENAI_LINK_SCRIPTS_PROPERTY GLOBAL PROPERTY LISTENAI_LINK_SCRIPTS)
    get_property(INCLUDE_DIRS TARGET listenai_interface PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    get_property(LISTENAI_IF_DEFINITIONS TARGET listenai_interface PROPERTY INTERFACE_COMPILE_DEFINITIONS)

    set(INCLUDE_FLAGS "")
    foreach(dir ${INCLUDE_DIRS})
        list(APPEND INCLUDE_FLAGS "-I${dir}")
    endforeach()

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
            COMMAND cp ${linker_script} ${CMAKE_BINARY_DIR}/linker.ld.pre
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

    add_dependencies(${LISTENAI_EXECUTABLE_NAME} generate_linker_script)
    target_link_options(${LISTENAI_EXECUTABLE_NAME} PRIVATE "-T${CMAKE_BINARY_DIR}/linker.ld")
endmacro()

macro(listenai_append_linker_script linker_script)
    set_property(GLOBAL APPEND PROPERTY LISTENAI_LINK_SCRIPTS ${linker_script})
endmacro()
