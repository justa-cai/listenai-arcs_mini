if (CONFIG_LINK_OPTION_GC_SECTIONS)
    add_link_options(-Wl,--gc-sections)
endif()

if (CONFIG_LINK_OPTION_ORPHAN_SECTIONS_ERROR)
    add_link_options(-Wl,--orphan-handling=error)
endif()

if (CONFIG_PRINT_MEMORY_USAGE)
    add_link_options(-Wl,--print-memory-usage)
endif()

if (CONFIG_LINK_OPTION_NOSYS_SPECS)
    add_link_options(--specs=nosys.specs)
endif()

if (CONFIG_LINK_OPTION_NANO_SPECS)
    add_link_options(--specs=nano.specs)
endif()

add_link_options(-Wl,--no-warn-rwx-segments)
add_link_options(-nostartfiles)
add_link_options(-static)
