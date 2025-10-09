/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include "timeval.h"
#include "FreeRTOS.h"
#include "task.h"

struct curltime Curl_now(void)
{
    /*
    ** time() returns the value of time in seconds since the Epoch.
    */
    struct curltime now;
    TickType_t ticks = xTaskGetTickCount();

    now.tv_sec = ticks / configTICK_RATE_HZ;
    now.tv_usec = (ticks % configTICK_RATE_HZ) * (1000000 / configTICK_RATE_HZ);
    return now;
}

/*
 * Returns: time difference in number of milliseconds. For too large diffs it
 * returns max value.
 *
 * @unittest: 1323
 */
timediff_t Curl_timediff(struct curltime newer, struct curltime older)
{
  timediff_t diff = (timediff_t)newer.tv_sec-older.tv_sec;
  if(diff >= (TIMEDIFF_T_MAX/1000))
    return TIMEDIFF_T_MAX;
  else if(diff <= (TIMEDIFF_T_MIN/1000))
    return TIMEDIFF_T_MIN;
  return diff * 1000 + (newer.tv_usec-older.tv_usec)/1000;
}

/*
 * Returns: time difference in number of milliseconds, rounded up.
 * For too large diffs it returns max value.
 */
timediff_t Curl_timediff_ceil(struct curltime newer, struct curltime older)
{
  timediff_t diff = (timediff_t)newer.tv_sec-older.tv_sec;
  if(diff >= (TIMEDIFF_T_MAX/1000))
    return TIMEDIFF_T_MAX;
  else if(diff <= (TIMEDIFF_T_MIN/1000))
    return TIMEDIFF_T_MIN;
  return diff * 1000 + (newer.tv_usec - older.tv_usec + 999)/1000;
}

/*
 * Returns: time difference in number of microseconds. For too large diffs it
 * returns max value.
 */
timediff_t Curl_timediff_us(struct curltime newer, struct curltime older)
{
  timediff_t diff = (timediff_t)newer.tv_sec-older.tv_sec;
  if(diff >= (TIMEDIFF_T_MAX/1000000))
    return TIMEDIFF_T_MAX;
  else if(diff <= (TIMEDIFF_T_MIN/1000000))
    return TIMEDIFF_T_MIN;
  return diff * 1000000 + newer.tv_usec-older.tv_usec;
}
