

if (CONFIG_LISA_BLUETOOTH_CLASSIC AND CONFIG_LISA_BLUETOOTH_BLE)
    add_subdirectory(bt)
elseif (CONFIG_LISA_BLUETOOTH_BLE)
    add_subdirectory(ble_peripheral)
else()
    message(FATAL_ERROR "Please enable LISA_BLUETOOTH_CLASSIC or LISA_BLUETOOTH_BLE")
endif()
