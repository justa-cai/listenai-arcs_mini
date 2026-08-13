file(GLOB_RECURSE LS_JPEG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

create_module_library(ls_jpeg "${LS_JPEG_SOURCES}")

target_include_directories(ls_jpeg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
