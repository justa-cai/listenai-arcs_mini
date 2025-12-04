include(ExternalProject)
add_dependencies(${LISTENAI_EXECUTABLE_NAME} boot)

# 允许外部指定生成 app.bin 的 target，默认为 mkhdr
if(NOT DEFINED LISTENAI_BOOT_APP_BIN_TARGET)
    set(LISTENAI_BOOT_APP_BIN_TARGET "mkhdr")
endif()

set(boot_bin_path ${CMAKE_BINARY_DIR}/boot/boot.bin)
set(app_bin_path ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}.bin)
set(MERGED_BINARY_PATH ${CMAKE_BINARY_DIR}/${LISTENAI_EXECUTABLE_NAME}_with_boot.bin)
set(FILL_DATA_PATH ${CMAKE_BINARY_DIR}/fill_data.bin)

math(EXPR FIRMWARE_SIZE "${CONFIG_BOOT_FLASH_SIZE}")

if (NOT DEFINED CONFIG_MEM_CONFIG)
    if (NOT DEFINED CONFIG_BOOT_CP_ENTRY)
        message(FATAL_ERROR "CONFIG_BOOT_CP_ENTRY is not defined")
    endif()
    math(EXPR CP_ENTRY "${CONFIG_BOOT_CP_ENTRY}")
    math(EXPR EXPECT_CP_ENTRY "0x30000000 + ${CONFIG_BOOT_FLASH_SIZE}")
    to_hex(${EXPECT_CP_ENTRY} EXPECT_CP_ENTRY_HEX)

    if (NOT ${CP_ENTRY} STREQUAL ${EXPECT_CP_ENTRY})
        message(FATAL_ERROR "CONFIG_BOOT_CP_ENTRY:${CONFIG_BOOT_CP_ENTRY} is not config correctly, expect entry:${EXPECT_CP_ENTRY_HEX}")
    endif()
else()
    math(EXPR CP_ENTRY "${CONFIG_MEM_FLASH_BASE} + ${CONFIG_BOOT_FLASH_SIZE}")
endif()

to_hex(${CP_ENTRY} CP_ENTRY_HEX)

get_cmake_property(VARS VARIABLES)

set(BOOT_CONFIGS "")
foreach(VAR IN LISTS VARS)
    if(VAR MATCHES "^CONFIG_BOOT")
        message(STATUS "${VAR} = ${${VAR}}")
        list(APPEND BOOT_CONFIGS "${VAR}=${${VAR}}")
    endif()
endforeach()
list(APPEND BOOT_CONFIGS "CONFIG_BOOT_CP_ENTRY=${CP_ENTRY_HEX}")

# 收集需要传递给 boot 的额外编译定义
set(BOOT_EXTRA_DEFINES "")
if(DEFINED CONFIG_MEM_FLASH_BASE)
    list(APPEND BOOT_EXTRA_DEFINES "CONFIG_MEM_FLASH_BASE=${CONFIG_MEM_FLASH_BASE}")
endif()

set(BOOT_APP_CONFIG_FILE ${CMAKE_BINARY_DIR}/boot/app.config)
foreach(config IN LISTS BOOT_CONFIGS)
    file(APPEND ${BOOT_APP_CONFIG_FILE} "${config}\n")
endforeach()

# 将列表转换为字符串传递
string(REPLACE ";" "|" BOOT_EXTRA_DEFINES_STR "${BOOT_EXTRA_DEFINES}")

ExternalProject_Add(
    boot
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/boot
    BINARY_DIR ${CMAKE_BINARY_DIR}/boot
    CMAKE_ARGS
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCONFIG_FILES=${BOOT_APP_CONFIG_FILE}
        -DBOOT_EXTRA_DEFINES=${BOOT_EXTRA_DEFINES_STR}
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .
)

add_custom_target(
    merge_boot
    ALL
    COMMAND truncate -s ${FIRMWARE_SIZE} ${boot_bin_path}
    COMMAND ${CMAKE_COMMAND} -E echo "-- Merging boot and app binary files"
    COMMAND ${CMAKE_COMMAND} -E cat ${boot_bin_path} ${app_bin_path} > ${MERGED_BINARY_PATH}
    COMMAND ${CMAKE_COMMAND} -E rename ${LISTENAI_EXECUTABLE_NAME}.bin ${LISTENAI_EXECUTABLE_NAME}.bin.without.boot
    COMMAND ${CMAKE_COMMAND} -E rename ${MERGED_BINARY_PATH} ${LISTENAI_EXECUTABLE_NAME}.bin

    DEPENDS boot ${LISTENAI_BOOT_APP_BIN_TARGET}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
