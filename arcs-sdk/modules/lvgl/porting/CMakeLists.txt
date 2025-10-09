listenai_library_sources(
    lv_port_disp.c
    lv_port_indev.c
)

listenai_library_sources_ifdef(CONFIG_LV_USE_GPU_CSK_DMA2D lv_gpu_csk_dma2d.c csk_dma2d_wrapper.c)

##A核只做代理服务，不编译lv_port_mem.c
if(CONFIG_HARTID EQUAL 1)
listenai_library_sources(lv_port_mem.c)
endif()

if(CONFIG_LV_USE_RPC)
add_subdirectory(func_proxy)
endif()

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})