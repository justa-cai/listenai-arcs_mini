# LISA Audio Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_AUDIO_DEVICE)

listenai_library_named(lisa_audio_device)


if(CONFIG_PORT_ARCS)
    listenai_library_sources(
        lisa_audio_arcs.c
        port/arcs/lisa_audio_record.c
        port/arcs/lisa_audio_play.c
        port/arcs/audio_adc_init.c
        port/arcs/audio_dac_init.c
    )
    listenai_include_directories(port/arcs)
endif()

if(CONFIG_PORT_VENUSA)
    listenai_library_sources(
        lisa_audio_venusa.c
        port/venusa/lisa_audio_record.c
        port/venusa/lisa_audio_play.c
        port/venusa/audio_adc_init.c
        port/venusa/audio_dac_init.c
    )
    listenai_include_directories(port/venusa)
endif()

listenai_include_directories(.)

endif() # CONFIG_LISA_AUDIO_DEVICE
