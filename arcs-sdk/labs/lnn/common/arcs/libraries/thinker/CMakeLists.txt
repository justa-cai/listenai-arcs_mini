set(THINKER_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libthinker.a)

add_library(thinker INTERFACE)

target_include_directories(thinker INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(thinker INTERFACE
    -Wl,--start-group
    ${THINKER_LIB_PATH}
    lunna
    m
    -Wl,--end-group
)
