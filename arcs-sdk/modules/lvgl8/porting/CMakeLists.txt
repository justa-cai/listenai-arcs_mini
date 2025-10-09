listenai_library_sources(
    lv_port_disp.c
    lv_port_indev.c
)

listenai_library_sources_ifdef(CONFIG_LV_USE_GPU_CSK_DMA2D lv_gpu_csk_dma2d.c)
listenai_library_sources_ifdef(CONFIG_LV_CSK_SW_LETTER lv_draw_sw_csk_letter.c)

listenai_library_sources(lv_port_mem.c)

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(CONFIG_LV_USE_RPC)
add_subdirectory(func_proxy)
endif()