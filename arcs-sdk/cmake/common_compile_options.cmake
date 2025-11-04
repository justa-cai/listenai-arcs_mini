if (CONFIG_DEBUG)
    add_compile_options(-g)
endif()

if (CONFIG_COMPILE_OPTION_OPTIMIZE_LEVEL_O0)
    add_compile_options(-O0)
endif()

if (CONFIG_COMPILE_OPTION_OPTIMIZE_LEVEL_O1)
    add_compile_options(-O1)
endif()

if (CONFIG_COMPILE_OPTION_OPTIMIZE_LEVEL_O2)
    add_compile_options(-O2)
endif()

if (CONFIG_COMPILE_OPTION_OPTIMIZE_LEVEL_O3)
    add_compile_options(-O3)
endif()

if (CONFIG_COMPILE_OPTION_OPTIMIZE_LEVEL_OS)
    add_compile_options(-Os)
endif()

if (CONFIG_COMPILE_OPTION_WARNING_AS_ERROR)
    add_compile_options(-Werror)
endif()

if (CONFIG_COMPILE_OPTION_WARNING_ALL)
    add_compile_options(-Wall)
endif()

if (CONFIG_COMPILE_OPTION_WARNING_DISABLE)
    add_compile_options(-w)
endif()

option(ENABLE_DEBUG_PATH "Enable debug path info" ON)

add_compile_options(-MMD)
add_compile_options(-Wno-comment)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>)
add_compile_options(-fno-common)
add_compile_options(-fno-builtin-printf -fno-builtin-puts)
add_compile_options(-fno-omit-frame-pointer -fno-optimize-sibling-calls)
add_compile_options(-ffunction-sections -fdata-sections -ffast-math)
add_compile_options(-fdiagnostics-color=always)
# add_compile_options(-mcmodel=medlow)

if (ENABLE_DEBUG_PATH)
    message(STATUS "ENABLE_DEBUG_PATH is ON")
else()
    message(STATUS "ENABLE_DEBUG_PATH is OFF")
    # 使用-ffile-prefix-map来减少调试信息中的路径长度，从而固定SRAM占用
    get_filename_component(ROOT_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
    add_compile_options("-ffile-prefix-map=${ROOT_PROJECT_DIR}=.")

    # 移除符号表中的build目录前缀
    get_filename_component(BUILD_DIR ${CMAKE_BINARY_DIR} ABSOLUTE)
    add_compile_options("-ffile-prefix-map=${BUILD_DIR}=")

    # 同时也处理工具链目录的路径
    if(DEFINED ENV{NUCLEI_TOOLCHAIN_PATH})
        add_compile_options("-ffile-prefix-map=$ENV{NUCLEI_TOOLCHAIN_PATH}=/toolchain")
    endif()
endif()

