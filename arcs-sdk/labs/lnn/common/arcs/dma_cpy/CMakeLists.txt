if(CONFIG_ALGO_DMA_CHANNEL_PERSISTENT)
    set(ARCS_DMA_CPY_SOURCE dma_cpy_arcs_ch_persistent.c)
else()
    set(ARCS_DMA_CPY_SOURCE dma_cpy_arcs.c)
endif()

if(CONFIG_ALGO_DMA_COPY_USE_DMA)
    set(ARCS_DMA_CPY_USE_DMA 1)
else()
    set(ARCS_DMA_CPY_USE_DMA 0)
endif()

add_library(arcs_dma_cpy STATIC ${ARCS_DMA_CPY_SOURCE})

target_include_directories(arcs_dma_cpy PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(arcs_dma_cpy PRIVATE
    $ENV{ARCS_BASE}/soc/arcs/hal/include
    $ENV{ARCS_BASE}/soc/arcs/hal/include/bsp
    $ENV{ARCS_BASE}/soc/arcs/hal/include/NMSIS/Core/Include
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/bsp
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/include
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/include/register
    $ENV{ARCS_BASE}/soc/arcs/hal/chip/arcs/driver/private_include
    $ENV{ARCS_BASE}/modules/freertos/include
    ${CMAKE_BINARY_DIR}/generated/include
)

target_compile_definitions(arcs_dma_cpy PRIVATE
    DMA_CPY_USE_DMA=${ARCS_DMA_CPY_USE_DMA}
)
