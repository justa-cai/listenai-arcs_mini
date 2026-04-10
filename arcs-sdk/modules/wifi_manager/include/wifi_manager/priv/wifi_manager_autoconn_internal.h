/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MGR_AUTOCONN_CYCLE_SUCCESS = 0,
    WIFI_MGR_AUTOCONN_CYCLE_CONNECT_FAIL,
    WIFI_MGR_AUTOCONN_CYCLE_NO_CANDIDATE,
} wifi_mgr_autoconn_cycle_result_t;

uint32_t wifi_mgr_autoconn_next_interval_ms(uint32_t base_interval_ms,
                                            uint32_t current_interval_ms,
                                            uint32_t no_candidate_streak,
                                            wifi_mgr_autoconn_cycle_result_t result,
                                            uint32_t max_interval_ms);

#ifdef __cplusplus
}
#endif
