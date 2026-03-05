# LISA Touch Driver
#
# Copyright (c) 2025, LISTENAI
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_LISA_TOUCH_DEVICE)

listenai_library_named(lisa_touch_device)

listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_CST328 lisa_touch_cst328.c)
listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_AXS15231B lisa_touch_axs15231b.c)
listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_FT5336 lisa_touch_ft5336.c)
listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_CST816D lisa_touch_cst816d.c)
listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_ST77921 lisa_touch_st77921.c)
listenai_library_sources_ifdef(CONFIG_LISA_TOUCH_ARCS_BL6133 lisa_touch_bl6133.c)

listenai_include_directories(.)

endif() # CONFIG_LISA_TOUCH_DEVICE
