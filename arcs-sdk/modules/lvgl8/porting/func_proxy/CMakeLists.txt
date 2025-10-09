if(CONFIG_HARTID EQUAL 0)
listenai_library_sources(lv_func_proxy_server.c)
else()
listenai_library_sources(lv_func_proxy.c)
endif()

listenai_include_directories(${CMAKE_CURRENT_SOURCE_DIR})