cmake_minimum_required(VERSION 3.19)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(BOARD MATCHES "^venusa")
    list(FIND CONFIG_FILES prj_venusa.conf _venusa_config_index)
    if(_venusa_config_index EQUAL -1)
        list(APPEND CONFIG_FILES prj_venusa.conf)
    endif()
    message(STATUS "---venusa append prj_venusa.conf---")
endif()

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE src/main.c)
