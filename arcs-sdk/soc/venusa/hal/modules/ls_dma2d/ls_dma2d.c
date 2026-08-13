#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include "ls_dma2d.h"
#include "log_print.h"
#include "fragment.h"
#include "Driver_DMA2D.h"
#include "cache.h"
#include "venusa_ap.h"


/*
    //Optimize the following operations
    for (int k = 0; k < width * height; k++)
    {
        *pBufBBB++ = *data++ - 128;
        *pBufGGG++ = *data++ - 128;
        *pBufRRR++ = *data++ - 128;
    }
*/
void test_performance_copy_calc_cpu(unsigned char *data, unsigned int width, unsigned int height, unsigned char *pBufBBB, unsigned char *pBufGGG, unsigned char *pBufRRR)
{
    // 23.2ms
    const uint32_t pixels = width * height;
    uint32_t i = 0;

    uint32_t *b32 = (uint32_t *)pBufBBB;
    uint32_t *g32 = (uint32_t *)pBufGGG;
    uint32_t *r32 = (uint32_t *)pBufRRR;

    // Process 4 pixels (12 bytes) per iteration
    while ((i + 3) < pixels) {
        const uint8_t *p = data + 3 * i;

        uint32_t w0 = *(const uint32_t*)(p);     
        uint32_t w1 = *(const uint32_t*)(p + 4); 
        uint32_t w2 = *(const uint32_t*)(p + 8); 

        uint32_t b = (w0         & 0x000000FF)        // b0
                   | ((w0 >> 16) & 0x0000FF00)        // b3
                   | (w1         & 0x00FF0000)        // b6
                   | ((w2 << 16) & 0xFF000000);       // b9

        uint32_t g = ((w0 >> 8)  & 0x000000FF)        // b1
                   | ((w1 << 8)  & 0x0000FF00)        // b4
                   | ((w1 >> 8)  & 0x00FF0000)        // b7
                   | ((w2 << 8)  & 0xFF000000);       // b10

        uint32_t r = ((w0 >>16)  & 0x000000FF)        // b2
                   | (w1         & 0x0000FF00)        // b5
                   | ((w2 <<16)  & 0x00FF0000)        // b8
                   | (w2         & 0xFF000000);       // b11

        // Subtract 128 from each byte via XOR 0x80
        *b32++ = b ^ 0x80808080u;
        *g32++ = g ^ 0x80808080u;
        *r32++ = r ^ 0x80808080u;

        i += 4;
    }
    
    // Handle remaining pixels (1 to 3)
    while (i < pixels) {
        const uint8_t *p = data + 3 * i;
        pBufBBB[i] = (p[0] ^ 0x80);
        pBufGGG[i] = (p[1] ^ 0x80);
        pBufRRR[i] = (p[2] ^ 0x80);
        i++;
    }
}


uint32_t dma2d_pixel_bitw(csk_image_format_t format)
{
    uint32_t bitw = 0;

    switch(format)
    {
        case csk_image_format_yuv444_packed:
        case csk_image_format_rgb888:
        case csk_image_format_bgr888:
            bitw = 3;
            break;

        case csk_image_format_yuv422_yuyv_packed:
        case csk_image_format_yuv422_uyvy_packed:
        case csk_image_format_yuv422_yvyu_packed:
        case csk_image_format_yuv422_vyuy_packed:
        case csk_image_format_rgb565:
        case csk_image_format_bgr565:
            bitw = 2;
            break;

        case csk_image_format_y8:
            bitw = 1;
            break;

        default:
            bitw = 1;
            break;
    }

    return bitw;
}

volatile uint32_t dma2d_image_event = 0;

#define DMA2D_MIN_SCALER_OUT_W 12
#define DMA2D_IMAGE_EVENT_WAIT_MAX_CNT 100000000U

static void dma2d_irq_callback(uint32_t event, void* workspace)
{
    dma2d_image_event = event;
    return;
}

#define VALID_PIXEL_NUM_MIN     2
int roi_region_conversion(int srcw, int srch, unsigned char **dst, int *dstw, int *dsth, image_format_t dstf, int *roix, int *roiy, int *roiw, int *roih)
{
    if (VALID_PIXEL_NUM_MIN > srcw || VALID_PIXEL_NUM_MIN > srch || VALID_PIXEL_NUM_MIN > *dstw || VALID_PIXEL_NUM_MIN > *dsth || VALID_PIXEL_NUM_MIN > *roiw || VALID_PIXEL_NUM_MIN > *roih)
    {
        CLOG("[%s][%d]error param,width&height must > %d\n", __FUNCTION__, __LINE__, VALID_PIXEL_NUM_MIN);
        return -1;
    }

    int roix_new = 0;
    int roiy_new = 0;
    int roiw_new = 0;
    int roih_new = 0;
    int dstw_new = 0;
    int dsth_new = 0;
    int dstx_new = 0;
    int dsty_new = 0;
    float ratio_w = 0;
    float ratio_h = 0;

    // ROI not all within the original map area
    if (!((0 <= *roix) && (srcw >= (*roix + *roiw)) && (0 <= *roiy) && (srch >= (*roiy + *roih))))
    {
//        CLOG("[%s][%d]ROI not all within the original map area,need update roi area\n", __FUNCTION__, __LINE__);
//        memset(*dst, 0, *dstw * *dsth * dma2d_pixel_bitw((csk_image_format_t)dstf));
        unsigned int dst_blklen = *dstw * *dsth * dma2d_pixel_bitw((csk_image_format_t)dstf);
        memset(*dst, 0, dst_blklen);
        HAL_FlushDCache_by_Addr((uint32_t*)*dst, dst_blklen);

        if (0 > *roix)
        {
            roix_new = 0;
            roiw_new = *roix + *roiw;
            if (0 >= roiw_new)
            {
                CLOG("[%s][%d]error param,0>=roix+roiw\n", __FUNCTION__, __LINE__);
                return -1;
            }
            else if (srcw < roiw_new)
            {
                roiw_new = srcw;
            }
        }
        else if (srcw < *roix)
        {
            CLOG("[%s][%d]error param,roix > srcw\n", __FUNCTION__, __LINE__);
            return -1;
        }
        else
        {
            roix_new = *roix;
            if (srcw  >= (*roix + *roiw))
                roiw_new = *roiw;
            else
                roiw_new = srcw - *roix;
        }

        if (0 > *roiy)
        {
            roiy_new = 0;
            roih_new = *roiy + *roih;
            if (0 >= roih_new)
            {
                CLOG("[%s][%d]error param,0>=roiy+roih\n", __FUNCTION__, __LINE__);
                return -1;
            }
            else if (srch < roih_new)
            {
                roih_new = srch;
            }
        }
        else if (srch < *roiy)
        {
            CLOG("[%s][%d]error param,roiy > srch\n", __FUNCTION__, __LINE__);
            return -1;
        }
        else
        {
            roiy_new = *roiy;
            if (srch  >= (*roiy + *roih))
                roih_new = *roih;
            else
                roih_new = srch - *roiy;
        }

        ratio_w = (float)*dstw / *roiw;
        ratio_h = (float)*dsth / *roih;

        dstx_new = (roix_new - *roix) * ratio_w;
        dsty_new = (roiy_new - *roiy) * ratio_h;
        dstw_new = roiw_new * ratio_w;
        dsth_new = roih_new * ratio_h;

        *dst += (*dstw * dsty_new + dstx_new)*dma2d_pixel_bitw((csk_image_format_t)dstf);

        *roix = roix_new;
        *roiy = roiy_new;
        *roiw = roiw_new;
        *roih = roih_new;
        *dstw = dstw_new;
        *dsth = dsth_new;
//        CLOG("[%s][%d]ROI area update-->(%d,%d,%d,%d),dst area update-->(%d,%d,%d,%d),dst_addr=%p\n", __FUNCTION__, __LINE__, *roix, *roiy, *roiw, *roih, dstx_new, dsty_new, *dstw, *dsth, *dst);
    }

    return 0;
}

/*
    use to crop (roix, roiy, roiw, roih) pic from (srcw, srch) and to zoom (dstw, dsth),img format from yuv422_yuyv to bgr888
*/
void ls_resize_bilinear_with_roi(unsigned char *src, int srcw, int srch, image_format_t srcf, unsigned char *dst, int dstw, int dsth, image_format_t dstf, int roix, int roiy, int roiw, int roih)
{
    int32_t ret = CSK_DRIVER_OK;
    uint32_t blk_len = 0;
    int dstw_stride = dstw;
    unsigned char *img_buff_src = src;
    unsigned char *img_buff_dst = dst;
    unsigned char *img_buff_src_line = NULL;
    unsigned char *img_buff_dst_line = NULL;
    csk_dma2d_ch_t chn = dma_2d_ch0;
    csk_dma2d_init_t dma2d_para;
    csk_dma_2d_image_cfg_t dma2d_img_cfg;
    cal_scaler_param_t scaler_param;
    memset(&dma2d_para, 0, sizeof(dma2d_para));
    memset(&dma2d_img_cfg, 0, sizeof(dma2d_img_cfg));
    memset(&scaler_param, 0, sizeof(scaler_param));

    // update roi area
    if (0 != roi_region_conversion(srcw, srch, &img_buff_dst, &dstw, &dsth, dstf, &roix, &roiy, &roiw, &roih))
    {
        CLOG("[%s][%d] roi_region_conversion failed\n", __FUNCTION__, __LINE__);
        return;
    }

    if (CSK_DRIVER_OK != (ret = DMA2D_Initialize()))
    {
        CLOG("[%s][%d]chn=%d DMA2D_Initialize failed,ret=%d", __FUNCTION__, __LINE__, chn, ret);
        return;
    }

    dma2d_para.dma_ch = chn;
    dma2d_para.tfr_mode = tfr_mode_m2m;
    dma2d_para.src_basic_unit = dma2d_sample_unit_word;
    dma2d_para.dst_basic_unit = dma2d_sample_unit_word;
    dma2d_para.src_inc_mode = inc_mode_increase;
    dma2d_para.dst_inc_mode = inc_mode_increase;
    dma2d_para.src_burst_len = dma2d_burst_len_16spl;
    dma2d_para.dst_burst_len = dma2d_burst_len_16spl;
    dma2d_para.flow_ctrl = dma2d_flow_ctrl_dma;
    dma2d_para.prio_lvl = prio_mode_vhigh;
    dma2d_para.handshake = hs_none;
    dma2d_para.rd_max_len = dma2d_ahb_burst_len_256byte;
    if (CSK_DRIVER_OK != (ret = DMA2D_Config(&dma2d_para, dma2d_irq_callback, NULL)))
    {
        CLOG("[%s][%d]chn=%d DMA2D_Config failed,ret=%d", __FUNCTION__, __LINE__, chn, ret);
        return;
    }

    dma2d_img_cfg.img_input.img_format = (csk_image_format_t)srcf;
    if (csk_image_format_yuv422_yuyv_packed <= dma2d_img_cfg.img_input.img_format && csk_image_format_yuv422_vyuy_packed >= dma2d_img_cfg.img_input.img_format)
    {
        FRAGMENT_ALIGN_DOWN(roix, 2);
        FRAGMENT_ALIGN_DOWN(roiw, 2);
    }
    dma2d_img_cfg.img_output.img_format = (csk_image_format_t)dstf;
    if (csk_image_format_yuv422_yuyv_packed <= dma2d_img_cfg.img_output.img_format && csk_image_format_yuv422_vyuy_packed >= dma2d_img_cfg.img_output.img_format)
    {
        FRAGMENT_ALIGN_DOWN(dstw, 2);
        FRAGMENT_ALIGN_DOWN(dstw_stride, 2);
    }

    // image fragment check
    scaler_param.partHori = 1;
    scaler_param.partVert = 1;
    scaler_param.reg_imwidthin = roiw;
    scaler_param.reg_imheightin = roih;
    scaler_param.reg_imwidthout = dstw;
    scaler_param.reg_imheightout = dsth;
    scaler_param.reg_merge_flag = 1;
    scaler_param.reg_scaler_flag = 1;
    // 传递真实格式，便于分片逻辑做 packed YUV422 偶数宽保护
    switch (srcf) {
        case csk_image_format_yuv422_yuyv_packed:
        case csk_image_format_yuv422_uyvy_packed:
        case csk_image_format_yuv422_yvyu_packed:
        case csk_image_format_yuv422_vyuy_packed:
            scaler_param.in_format = YUV422P; // 让分片逻辑走 YUV422 偶数宽分支
            break;
        default:
            scaler_param.in_format = RGB888;
            break;
    }
    switch (dstf) {
        case csk_image_format_yuv422_yuyv_packed:
        case csk_image_format_yuv422_uyvy_packed:
        case csk_image_format_yuv422_yvyu_packed:
        case csk_image_format_yuv422_vyuy_packed:
            scaler_param.out_format = YUV422P;
            break;
        default:
            scaler_param.out_format = RGB888;
            break;
    }
    scaler_param.width_max = 256;
    scaler_param.height_max = 4096;
    dma2d_scaler_image_fragment(&scaler_param);

    int total_frames = scaler_param.partHori * scaler_param.partVert;
    int frame_index = 0;
//    CLOG("Total frames: %d (H:%d -- V:%d)\n", total_frames, scaler_param.partHori, scaler_param.partVert);

    dma2d_img_cfg.img_input.img_line_stride = srcw*dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    dma2d_img_cfg.img_output.img_line_stride = dstw_stride*dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    dma2d_img_cfg.img_crop_en = csk_func_enable;
    dma2d_img_cfg.img_scaler_en = csk_func_enable;
    dma2d_img_cfg.img_fragment_en = csk_func_enable;
    dma2d_img_cfg.img_scaler_param.scaler_flg = scaler_param.reg_scaler_flag;
    dma2d_img_cfg.img_scaler_param.scaler_x = scaler_param.reg_scaler_up_x;
    dma2d_img_cfg.img_scaler_param.scaler_y = scaler_param.reg_scaler_up_y;
    dma2d_img_cfg.img_scaler_param.merge_flg = scaler_param.reg_merge_flag;
    dma2d_img_cfg.img_scaler_param.merge_x = scaler_param.reg_hori_num/2;
    dma2d_img_cfg.img_scaler_param.merge_y = scaler_param.reg_vert_num/2;
    dma2d_img_cfg.img_scaler_param.step_phase_x = scaler_param.reg_phasestep_x;
    dma2d_img_cfg.img_scaler_param.step_phase_y = scaler_param.reg_phasestep_y;
    img_buff_src += (roiy*srcw+roix)*dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
    for (frame_index = 0; frame_index < total_frames; frame_index++)
    {
        int hori_idx = frame_index % scaler_param.partHori;
        int vert_idx = frame_index / scaler_param.partHori;
        dma2d_img_cfg.img_scaler_param.last_tile = ((frame_index + 1) % scaler_param.partHori == 0) ? 1 : 0;
        dma2d_img_cfg.img_scaler_param.last_band = (frame_index >= scaler_param.partHori * (scaler_param.partVert - 1)) ? 1 : 0;
        dma2d_img_cfg.img_scaler_param.col_overlap_flg = scaler_param.reg_SkipFristSampleFlag_list[hori_idx];
        dma2d_img_cfg.img_scaler_param.start_phase_x = scaler_param.reg_start_phase_x_list[hori_idx];
        dma2d_img_cfg.img_scaler_param.start_phase_y = scaler_param.reg_start_phase_y_list[vert_idx];

//        CLOG("frame_index=%d(scaler=%u-%d-%d, merge=%u-%d-%d, lasttile=%u,lastband=%u,overlap=%u,step=%u-%u, start=%u-%u)\n", frame_index, dma2d_img_cfg.img_scaler_param.scaler_flg, dma2d_img_cfg.img_scaler_param.scaler_x, dma2d_img_cfg.img_scaler_param.scaler_y,
//               dma2d_img_cfg.img_scaler_param.merge_flg, dma2d_img_cfg.img_scaler_param.merge_x, dma2d_img_cfg.img_scaler_param.merge_y, dma2d_img_cfg.img_scaler_param.last_tile,
//               dma2d_img_cfg.img_scaler_param.last_band, dma2d_img_cfg.img_scaler_param.col_overlap_flg, dma2d_img_cfg.img_scaler_param.step_phase_x, dma2d_img_cfg.img_scaler_param.step_phase_y,
//               dma2d_img_cfg.img_scaler_param.start_phase_x, dma2d_img_cfg.img_scaler_param.start_phase_y);

        // 水平方向调整
        if (hori_idx > 0)
        {  // 非第一个水平分割块
            if (scaler_param.reg_merge_flag == 1 && scaler_param.reg_scaler_flag == 1)
            {
                // YUV422P格式特殊处理：直接使用原始值，不做调整
                if (scaler_param.in_format == YUV422P || scaler_param.out_format == YUV422P)
                {
                    dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
                } 
                else 
                {
                    if (scaler_param.reg_scalerstart_x_overlap[hori_idx] >= 1)
                        dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
                    else
                        dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx] - 1 * scaler_param.reg_hori_num;
                }
            }
            else if (scaler_param.reg_merge_flag == 0 && scaler_param.reg_scaler_flag == 1)
            {
                // YUV422P格式特殊处理：直接使用原始值，不做调整
                if (scaler_param.in_format == YUV422P || scaler_param.out_format == YUV422P)
                {
                    dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
                }
                else
                {
                    if (scaler_param.reg_scalerstart_x_overlap[hori_idx] >= 1)
                        dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
                    else
                        dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx] - 1;
                }
            }
            else
            {
                dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
            }
        }
        else
        {
            dma2d_img_cfg.img_input.img_width = scaler_param.reg_imwidthin_list[hori_idx];
        }

        // 垂直方向调整
        if (vert_idx > 0) 
        {  // 非第一个垂直分割块
            if (scaler_param.reg_merge_flag == 1 && scaler_param.reg_scaler_flag == 1)
            {
                if (scaler_param.reg_scalerstart_y_overlap[vert_idx] >= 1)
                    dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx];
                else
                    dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx] - 1 * scaler_param.reg_vert_num;
            } 
            else if (scaler_param.reg_merge_flag == 0 && scaler_param.reg_scaler_flag == 1) 
            {
                if (scaler_param.reg_scalerstart_y_overlap[vert_idx] == 1)
                    dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx];
                else
                    dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx] - 1;
            }
            else
            {
                dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx];
            }
        } 
        else
        {
            dma2d_img_cfg.img_input.img_height = scaler_param.reg_imheightin_list[vert_idx];
        }
        dma2d_img_cfg.img_output.img_width = scaler_param.reg_imwidthout_list[hori_idx];
        dma2d_img_cfg.img_output.img_height = scaler_param.reg_imheightout_list[vert_idx];

        if (csk_func_enable == dma2d_img_cfg.img_scaler_en &&
            dma2d_img_cfg.img_input.img_width != dma2d_img_cfg.img_output.img_width &&
            dma2d_img_cfg.img_output.img_width < DMA2D_MIN_SCALER_OUT_W)
        {
            CLOG("[%s][%d]chn=%d skip unstable tiny scaler out width: input(w=%u,h=%u) output(w=%u,h=%u), frame=%d/%d",
                 __FUNCTION__, __LINE__, chn,
                 dma2d_img_cfg.img_input.img_width, dma2d_img_cfg.img_input.img_height,
                 dma2d_img_cfg.img_output.img_width, dma2d_img_cfg.img_output.img_height,
                 frame_index, total_frames);
            return;
        }

        blk_len = dma2d_img_cfg.img_output.img_width * dma2d_img_cfg.img_output.img_height * dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);

//        CLOG("input(w=%u,h=%u), output(w=%u,h=%u), blklen=%u, srcaddr=%#x, dstaddr=%#x\n", dma2d_img_cfg.img_input.img_width, dma2d_img_cfg.img_input.img_height, dma2d_img_cfg.img_output.img_width,
//               dma2d_img_cfg.img_output.img_height, blk_len, img_buff_src, img_buff_dst);

        if (CSK_DRIVER_OK != (ret = DMA2D_Image_Config_Extend(chn, &dma2d_img_cfg)))
        {
            CLOG("[%s][%d]chn=%d DMA2D_Image_Config_Extend failed,ret=%d", __FUNCTION__, __LINE__, chn, ret);
            return;
        }

        if (CSK_DRIVER_OK != (ret = DMA2D_Start_Normal(chn, img_buff_src, img_buff_dst, blk_len)))
        {
            CLOG("[%s][%d]chn=%d DMA2D_Start_Normal failed,ret=%d", __FUNCTION__, __LINE__, chn, ret);
            return;
        }

        uint32_t wait_cnt = 0;
        while (!dma2d_image_event && wait_cnt < DMA2D_IMAGE_EVENT_WAIT_MAX_CNT)
        {
            wait_cnt++;
        }

        if (!dma2d_image_event)
        {
            CLOG("[%s][%d]chn=%d dma2d wait image event timeout, frame=%d/%d", __FUNCTION__, __LINE__, chn, frame_index, total_frames);
            if (CSK_DRIVER_OK != (ret = DMA2D_Stop(chn)))
            {
                CLOG("[%s][%d]chn=%d DMA2D_Stop failed after timeout,ret=%d", __FUNCTION__, __LINE__, chn, ret);
            }
            return;
        }

        dma2d_image_event = 0;
    
        if (CSK_DRIVER_OK != (ret = DMA2D_Stop(chn)))
        {
            CLOG("[%s][%d]chn=%d DMA2D_Stop failed,ret=%d", __FUNCTION__, __LINE__, chn, ret);
            return;
        }

        if (0 == frame_index % scaler_param.partHori)
        {
            img_buff_src_line = img_buff_src;
            img_buff_dst_line = img_buff_dst;
        }

        if (((frame_index+1) % scaler_param.partHori == 0) && ((frame_index+1)/scaler_param.partHori > 0))
        {
            if(scaler_param.reg_merge_flag == 1 && scaler_param.reg_scaler_flag == 1)
            {
                if(scaler_param.reg_scalerstart_y_overlap[(frame_index+1)/scaler_param.partHori] >= 1)
                    img_buff_src = img_buff_src_line + dma2d_img_cfg.img_input.img_line_stride*(dma2d_img_cfg.img_input.img_height - 1*scaler_param.reg_vert_num);
                else
                    img_buff_src = img_buff_src_line + dma2d_img_cfg.img_input.img_line_stride*dma2d_img_cfg.img_input.img_height;
            }
            else if(scaler_param.reg_merge_flag == 0 && scaler_param.reg_scaler_flag == 1)
            {
                if(scaler_param.reg_scalerstart_y_overlap[(frame_index+1)/scaler_param.partHori] == 1)
                    img_buff_src = img_buff_src_line + dma2d_img_cfg.img_input.img_line_stride*(dma2d_img_cfg.img_input.img_height - 1);
                else
                    img_buff_src = img_buff_src_line + dma2d_img_cfg.img_input.img_line_stride*dma2d_img_cfg.img_input.img_height;
            }
            else
            {
                img_buff_src = img_buff_src_line + dma2d_img_cfg.img_input.img_line_stride*dma2d_img_cfg.img_input.img_height;
            }
        }
        else
        {
            if(scaler_param.reg_merge_flag == 1 && scaler_param.reg_scaler_flag == 1)
                img_buff_src = img_buff_src + dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format) - dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format)*scaler_param.reg_scalerstart_x_overlap[(frame_index+1)%scaler_param.partHori];
            else if(scaler_param.reg_merge_flag == 0 && scaler_param.reg_scaler_flag == 1)
                img_buff_src = img_buff_src + dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format) - dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format)*scaler_param.reg_scalerstart_x_overlap[(frame_index+1)%scaler_param.partHori];
            else
                img_buff_src = img_buff_src + dma2d_img_cfg.img_input.img_width * dma2d_pixel_bitw(dma2d_img_cfg.img_input.img_format);
        }

       if(((frame_index+1) % scaler_param.partHori == 0) && ((frame_index+1)/scaler_param.partHori > 0))
           img_buff_dst = img_buff_dst_line + dma2d_img_cfg.img_output.img_height*dma2d_img_cfg.img_output.img_line_stride;
       else
           img_buff_dst = img_buff_dst + dma2d_img_cfg.img_output.img_width*dma2d_pixel_bitw(dma2d_img_cfg.img_output.img_format);
    }

//    MInvalDCache();

//    CLOG("[%s][%d]chn=%d source:0x%x -> destination:0x%x crop-scaler success!!!", __FUNCTION__, __LINE__, chn, src, dst);

    return;
}

