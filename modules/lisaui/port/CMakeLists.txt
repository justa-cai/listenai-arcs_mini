# 添加该目录到主库的包含路径
listenai_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CONFIG_LISAUI_ENV_ARCS_SDK)
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/lisaui_arcs.ld)
else()
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/lisaui.ld)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE -T ${LINKER_SCRIPT})
