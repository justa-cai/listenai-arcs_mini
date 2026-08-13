
file(GLOB UNITY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

add_compile_definitions(UNITY_INCLUDE_CONFIG_H)

create_module_library(unity "${UNITY_SOURCES}")

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
