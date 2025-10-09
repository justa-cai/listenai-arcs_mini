cmake_minimum_required(VERSION 3.10)

# 检查是否在扫描笔工程中构建
if(DEFINED LISTENAI_SDK_PATH)
    # 扫描笔工程环境下的构建
    listenai_library_named(ebus)
    listenai_library_sources(
        ebus.c
    )

    listenai_include_directories(
        ./inlcude
        ./port
    )
else()
    add_library(ebus STATIC "")

    target_include_directories(ebus PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_sources(ebus PRIVATE 
        ./ebus.c
    )

    if(CONFIG_EBUS_ENV_OS_POSIX)
        target_link_libraries(ebus PRIVATE pthread)
    else()
        target_link_libraries(ebus PUBLIC freertos)
    endif()
    
    if(DEFINED LISTENAI_CMAKE_PATH)
    listenai_link_libraries(ebus)
    endif()
endif()
# else()
#     # Linux环境下的构建
#     project(ebus C)
    
#     # 设置C标准
#     set(CMAKE_C_STANDARD 99)
#     set(CMAKE_C_STANDARD_REQUIRED ON)
    
#     # 添加配置选项
#     option(EBUS_ENV_OS_POSIX "使用POSIX环境" ON)
#     option(EBUS_ENV_OS_FREERTOS "使用FreeRTOS环境" OFF)
    
#     # 根据选项设置编译定义
#     if(EBUS_ENV_OS_POSIX)
#         add_definitions(-DCONFIG_EBUS_ENV_OS_POSIX=1)
#     elseif(EBUS_ENV_OS_FREERTOS)
#         add_definitions(-DCONFIG_EBUS_ENV_OS_FREERTOS=1)
#     endif()
    
#     # 添加源文件
#     add_library(ebus STATIC
#         ebus.c
#     )
    
#     # 添加头文件路径
#     target_include_directories(ebus PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}/inlcude
#         ${CMAKE_CURRENT_SOURCE_DIR}/port
#     )
    
#     # 如果是POSIX环境，链接pthread库
#     if(EBUS_ENV_OS_POSIX)
#         target_link_libraries(ebus PRIVATE pthread)
#     endif()
    
#     # 安装目标
#     install(TARGETS ebus
#         ARCHIVE DESTINATION lib
#         LIBRARY DESTINATION lib
#     )
    
#     install(DIRECTORY inlcude/
#         DESTINATION include
#         FILES_MATCHING PATTERN "*.h"
#     )
# endif()

# # 为POSIX环境创建port/posix目录下的实现
# if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/port/posix")
#     file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/port/posix")
# endif()

# # 为FreeRTOS环境创建port/freertos目录下的实现
# if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/port/freertos")
#     file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/port/freertos")
# endif()
