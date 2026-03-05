# 版本管理已移至 cmake/version.cmake
# 由主 CMakeLists.txt 在早期阶段包含
add_subdirectory(${CHIP})

if (DEFINED CONFIG_BOOT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/boot.cmake)
endif()
