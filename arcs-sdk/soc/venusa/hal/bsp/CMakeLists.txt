# BSP (Board Support Package) for VENUSA
file(GLOB_RECURSE BSP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

create_module_library(bsp "${BSP_SOURCES}")

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
