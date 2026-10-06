# THINKER_LIB=log 使用 libthinker_log.a（逐算子执行日志），用于定位板上挂死算子
if(DEFINED ENV{THINKER_LIB} AND "$ENV{THINKER_LIB}" STREQUAL "log")
    set(THINKER_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libthinker_log.a)
else()
    set(THINKER_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libthinker.a)
endif()

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
