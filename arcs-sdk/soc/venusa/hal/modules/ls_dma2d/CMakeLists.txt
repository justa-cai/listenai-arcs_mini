file(GLOB_RECURSE LS_DMA2D_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

create_module_library(ls_dma2d "${LS_DMA2D_SOURCES}")

target_include_directories(ls_dma2d PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TOPDIR}/chip/venusa/modules/fragment
)
