# =============================================================================
# Voice Assistant Application Build Configuration
# =============================================================================

listenai_library_named(app)

listenai_include_directories(./)

# Core application source files
listenai_library_sources(
    ./main.c
    ./app_algo.c
    ./app_client.c
    ./comm_service.c
)

add_subdirectory(alarm)
add_subdirectory(audio)
add_subdirectory(ble)
add_subdirectory(libc)
add_subdirectory(battery)
add_subdirectory(config)
add_subdirectory(controller)
add_subdirectory(system)
add_subdirectory(display)
add_subdirectory(led)
add_subdirectory(button)
add_subdirectory(fs)
add_subdirectory(kv)
add_subdirectory(flash)
add_subdirectory(video)
add_subdirectory(tone)
add_subdirectory(player)
add_subdirectory(cmd)
add_subdirectory(usb)
add_subdirectory(wifi)
add_subdirectory(cloud)
add_subdirectory(shell)
add_subdirectory(workqueue)
add_subdirectory(power)
add_subdirectory(sdemmc)
add_subdirectory(utils)
add_subdirectory(ota)
