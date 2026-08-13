cmake_minimum_required(VERSION 3.13)

set(LISTENAI_MODULES_DIR_LIST
    $ENV{ARCS_BASE}
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(listenai-cmake REQUIRED HINTS $ENV{ARCS_BASE})

project(app-lua)

listenai_add_executable(${PROJECT_NAME})
add_subdirectory(components)

# ---------- Auto-generate Lua driver and LVGL bindings at build time ----------
set(BINDINGS_DIR ${CMAKE_CURRENT_BINARY_DIR}/bindings)
set(DRIVERS_DIR  $ENV{ARCS_BASE}/drivers)
set(GEN_SCRIPT   ${CMAKE_CURRENT_SOURCE_DIR}/gen_lua_bindings.py)
set(LVGL_DIR     $ENV{ARCS_BASE}/modules/lvgl8)
set(LVGL_GEN_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/gen_lvgl_bindings.py)
set(LISA_BINDING_MODULES gpio pwm adc wdt rtc hwtimer flash spi i2c uart)
set(LISA_DRIVER_MODULES ${LISA_BINDING_MODULES} display touch)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(LUA_DRIVER_BINDING_SOURCES)
set(LUA_DRIVER_BINDING_DEPENDS ${GEN_SCRIPT})
foreach(mod ${LISA_BINDING_MODULES})
    list(APPEND LUA_DRIVER_BINDING_SOURCES ${BINDINGS_DIR}/lua_lisa_${mod}.c)
    list(APPEND LUA_DRIVER_BINDING_DEPENDS ${DRIVERS_DIR}/lisa_${mod}/lisa_${mod}.h)
endforeach()
set(LUA_DRIVER_BINDING_REGISTRY ${BINDINGS_DIR}/lua_lisa_all.h)

file(GLOB_RECURSE LVGL_BINDING_HEADER_DEPENDS CONFIGURE_DEPENDS
    ${LVGL_DIR}/*.h
)
set(LVGL_BINDING_SOURCE ${BINDINGS_DIR}/lua_lvgl.c)
set(LVGL_BINDING_REGISTRY ${BINDINGS_DIR}/lua_lvgl_all.h)

set_source_files_properties(
    ${LUA_DRIVER_BINDING_SOURCES}
    ${LVGL_BINDING_SOURCE}
    PROPERTIES GENERATED TRUE
)

add_custom_command(
    OUTPUT ${LUA_DRIVER_BINDING_SOURCES} ${LUA_DRIVER_BINDING_REGISTRY}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BINDINGS_DIR}
    COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT}
            --drivers-dir ${DRIVERS_DIR}
            --output-dir  ${BINDINGS_DIR}
    DEPENDS ${LUA_DRIVER_BINDING_DEPENDS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Lua driver bindings"
    VERBATIM
)

add_custom_command(
    OUTPUT ${LVGL_BINDING_SOURCE} ${LVGL_BINDING_REGISTRY}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BINDINGS_DIR}
    COMMAND ${Python3_EXECUTABLE} ${LVGL_GEN_SCRIPT}
            --lvgl-dir  ${LVGL_DIR}
            --output-dir ${BINDINGS_DIR}
    DEPENDS ${LVGL_GEN_SCRIPT} ${LVGL_BINDING_HEADER_DEPENDS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Lua LVGL bindings"
    VERBATIM
)

add_custom_target(lua_binding_lvgl_bindings
    DEPENDS
        ${LUA_DRIVER_BINDING_SOURCES}
        ${LUA_DRIVER_BINDING_REGISTRY}
        ${LVGL_BINDING_SOURCE}
        ${LVGL_BINDING_REGISTRY}
)
add_dependencies(${PROJECT_NAME} lua_binding_lvgl_bindings)

target_sources(${PROJECT_NAME} PRIVATE
    src/main.c
    src/usb_descriptors.c
    ${LUA_DRIVER_BINDING_SOURCES}
    ${LVGL_BINDING_SOURCE}
)

listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BINDINGS_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/components/lua
)

# Driver headers needed by generated Lua bindings and the LVGL display/touch port.
foreach(mod ${LISA_DRIVER_MODULES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${DRIVERS_DIR}/lisa_${mod})
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${LVGL_DIR}
    ${LVGL_DIR}/src
    ${LVGL_DIR}/porting
)
