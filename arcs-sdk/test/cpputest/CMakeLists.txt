cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(arcs)
listenai_include_directories(
    ./
)
listenai_add_executable(${PROJECT_NAME})


target_sources(${PROJECT_NAME} PRIVATE 
    test_gpio.cpp
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE CppUTest)
target_link_libraries(${PROJECT_NAME} PRIVATE CppUTestExt)
