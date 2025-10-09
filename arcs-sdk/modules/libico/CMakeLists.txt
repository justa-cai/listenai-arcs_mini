
if (CONFIG_SDK_MODULE_ICO)
listenai_link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/libico.a)
listenai_include_directories(
  ./include
)
endif()
