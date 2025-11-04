
if(DEFINED CONFIG_BLE_PROFILE_DISC)
    add_subdirectory(disc)
endif()

if(DEFINED CONFIG_BLE_PROFILE_DISS)
    add_subdirectory(diss)
endif()
    