add_subdirectory(game)

# sim 下不编译 llm(产品 UI): 其 models/presenters 深度依赖固件 API(FreeRTOS/kv/BLE/romfs),
# PC 上无法编译; 设备构建才编译完整产品 UI。
if(NOT LISA_UI_IS_SIM)
    add_subdirectory(llm)
endif()
