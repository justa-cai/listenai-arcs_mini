if (DEFINED CONFIG_LISA_OS)
    listenai_library_named(lisa_os)

    listenai_library_sources(
        lisa_mem.c
        lisa_queue.c
        lisa_thread.c
        lisa_timer.c
        lisa_time.c
        lisa_semaphore.c
        lisa_mutex.c
    )

    listenai_include_directories(inc)

endif()
