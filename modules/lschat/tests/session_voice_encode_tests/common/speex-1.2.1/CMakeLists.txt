set(LIB_NAME speex)
file(
    GLOB
    LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/libspeex/*.c
)

add_library(${LIB_NAME} STATIC ${LIB_SRCS})
target_link_libraries(${LIB_NAME} PRIVATE lisa_os)
target_include_directories(${LIB_NAME} PRIVATE lisa_porting)
target_include_directories(${LIB_NAME} PUBLIC include/speex)
target_include_directories(${LIB_NAME} PUBLIC include)
target_compile_options(${LIB_NAME} PRIVATE -DHAVE_CONFIG_H -Wno-shift-negative-value -Wno-unused-function)