cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)

listenai_add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    test_lisa_audio.c
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../common/mock_lisa_gpio mock_lisa_gpio_build)

target_link_libraries(${PROJECT_NAME} PRIVATE unity mock_lisa_gpio)
