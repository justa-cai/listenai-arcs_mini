cmake_minimum_required(VERSION 3.20)

project(lvgl_sim C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Support large static arrays (for 7MB Chinese font)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcmodel=medium")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcmodel=medium")
endif()

option(LISA_UI_SIM "run lisa ui in sim" ON)

add_compile_options(-g)

find_package(SDL2 REQUIRED SDL2)
include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}/../
)

add_executable(lisa_ui_app_sim main.c)

target_link_libraries(
    lisa_ui_app_sim

    PRIVATE
    lisa_ui_app
)

add_subdirectory(../ lisa_ui)
