#!/bin/bash

ar x liblvgl_demo_benchmark.a
ar x liblvgl_demo_stress.a
ar x liblvgl_extra.a
ar x liblvgl_widgets.a
ar x liblvgl_demo_keypad.a
ar x liblvgl_demo_touch.a
ar x liblvgl_font.a
ar x liblvgl_demo_music.a
ar x liblvgl_demo_widgets.a
ar x liblvgl_hal.a
ar x liblvgl_core.a
ar x liblvgl_demo_smartwatch.a
ar x liblvgl_draw.a
ar x liblvgl_misc.a

sync
ar rc liblvgl.a *.o


