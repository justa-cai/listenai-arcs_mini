# # 设置cmake最低版本要求
# cmake_minimum_required(VERSION 3.13)

# # 定义cjson项目
# project(cjson)

# # 定义接口库
# add_library(cjson_interface INTERFACE)

# # 将cJSON.c文件添加到接口库中
# target_sources(cjson_interface INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}/cJSON.c
# )

# # 包含cJSON头文件目录
# target_include_directories(cjson_interface INTERFACE
#     ${CMAKE_CURRENT_SOURCE_DIR}
# )

if (DEFINED CONFIG_CJSON)
    SET(SDK_MODULES_CJSON_PATH ${CMAKE_CURRENT_SOURCE_DIR})
    listenai_library_named(modules_cjson)

        file(GLOB_RECURSE SOURCES
            ${SDK_MODULES_CJSON_PATH}/cJSON.c
        )
        listenai_library_sources(${SOURCES})

        listenai_include_directories(${SDK_MODULES_CJSON_PATH}/)

        if (DEFINED CONFIG_CJSON_CUSTOM_INCLUDE)
            target_compile_definitions(modules_cjson PUBLIC -DCJSON_CUSTOM_INCLUDE="${CONFIG_CJSON_CUSTOM_INCLUDE_PATH}")
        endif()
endif()
