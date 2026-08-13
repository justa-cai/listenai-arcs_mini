set(LUNNA_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libluna.a)
set(LUNNA_EXT_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/liblunaext.a)
set(LUNNA_ARCS_DRIVER_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libarcsdriver.a)

add_library(lunna INTERFACE)

target_include_directories(lunna INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lunna INTERFACE
    ${LUNNA_EXT_LIB_PATH}
    ${LUNNA_ARCS_DRIVER_LIB_PATH}
    ${LUNNA_LIB_PATH}
)
