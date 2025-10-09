if (DEFINED CONFIG_IRQ_PROXY)
    listenai_library_named(irq_proxy)
    listenai_library_sources(
        irq_proxy.c
        dma_irq_proxy.c
        func_proxy.c
    )

    listenai_include_directories(.)
endif()
