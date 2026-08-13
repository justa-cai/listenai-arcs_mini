file(GLOB_RECURSE FRAGMENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

create_module_library(fragment "${FRAGMENT_SOURCES}")

target_include_directories(fragment PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
