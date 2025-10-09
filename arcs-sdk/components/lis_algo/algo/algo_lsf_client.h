/*
 * @Author: fhhao fhhao@listeanai.com
 * @Date: 2025-03-05 19:40:53
 * @LastEditors: fhhao fhhao@listeanai.com
 * @LastEditTime: 2025-03-24 16:23:35
 * @FilePath: /toycloud-cp/projects/scanpen-cp/src/algo/algo_lsf_client.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef ALGO_LSF_CLIENT_H
#define ALGO_LSF_CLIENT_H

#include <stdint.h>

typedef enum
{
	ap2cp_play_stream_id = 0,
    cp2ap_record_stream_id,
    cp2ap_scan_stream_id,
    cp2ap_xtts_stream_id,
    cp2ap_trans_stream_id,
    cp2ap_scan_adjust_stream_id,
} ic_stream_id_e;


void algo_lsf_client_start(void);
void algo_lsf_ocr_init(void);
void algo_lsf_xtts_init(void);
void algo_lsf_trans_init(void);

#endif /* ALGO_LSF_CLIENT_H */
