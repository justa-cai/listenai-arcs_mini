set(LIBRARY_NAME elmfat)
add_library(${LIBRARY_NAME} STATIC "")

set(LIBRARY_INLCUDE_DIR ${CMAKE_BINARY_DIR}/generated/include/${LIBRARY_NAME})
set(PUBLIC_HEADER
    "ff.h"
    "diskio.h"
)
foreach(file IN LISTS PUBLIC_HEADER)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/fatfs/source/${file} ${LIBRARY_INLCUDE_DIR}/${file} COPYONLY)
endforeach()

target_include_directories(${LIBRARY_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBRARY_INLCUDE_DIR}
)

target_sources(${LIBRARY_NAME} PRIVATE 
                      fatfs.c 
                      fatfs_diskio.c)

if (CONFIG_FS_ENV_OS_FREERTOS)
target_compile_definitions(${LIBRARY_NAME} PRIVATE OS_TYPE=3)
endif()
target_link_libraries(${LIBRARY_NAME} PUBLIC disk)

if(DEFINED LISTENAI_CMAKE_PATH)
listenai_link_libraries(${LIBRARY_NAME})
endif()

