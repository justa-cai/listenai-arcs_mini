#include "img_converters.h"
#include "Driver_Blender.h"
#include "check.h"
#include "csk_driver.h"

typedef struct
{
    void *back_addr;
    void *fore_addr;
    void *mask_addr;
    void *out_addr;
    uint32_t back_size;     // word
    uint32_t fore_size;     // word
    uint32_t mask_size;     // word
    uint32_t out_size;      // word
}DMA_CfgTypeDef;


static uint32_t blender_rgb888_pixel_sw(uint32_t rgb888, uint32_t color, uint8_t alpha)
{
    uint32_t r, g, b;

    //VIDEO_LOG("rgb888=0x%x, color=0x%x, alpha=0x%x", rgb888, color, alpha);

    r = (((color >> 16) & 0xFF) * alpha) + (((rgb888 >> 16) & 0xFF) * (0xFF - alpha));
    g = (((color >> 8) & 0xFF) * alpha) + (((rgb888 >> 8) & 0xFF) * (0xFF - alpha));
    b = ((color & 0xFF) * alpha) + ((rgb888 & 0xFF) * (0xFF - alpha));

    r = (r > 0x8000) ? (((r >> 8) & 0xFF) + 1) : ((r >> 8) & 0xFF);
    g = (g > 0x8000) ? (((g >> 8) & 0xFF) + 1) : ((g >> 8) & 0xFF);
    b = (b > 0x8000) ? (((b >> 8) & 0xFF) + 1) : ((b >> 8) & 0xFF);

    //VIDEO_LOG("[%s:%d] r=0x%x, g=0x%x, b=0x%x", __func__, __LINE__, r, g, b);

    return (((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
}


int img_blender_sw(Blender_InitTypeDef *pblender_cfg, void *dma_cfg, uint8_t *pout)
{
    int32_t ret = FAILURE;
    DMA_CfgTypeDef *pdma_cfg = (DMA_CfgTypeDef *)dma_cfg;
    uint8_t *pback_buf = pdma_cfg->back_addr;
    uint8_t *pfore_buf = pdma_cfg->fore_addr;
    uint8_t *pmask_buf = pdma_cfg->mask_addr;
    uint8_t *pblender_buf = pout;
    uint32_t back_rgb888, fore_rgb888, blender_rgb888;
    uint32_t color = 0;
    uint8_t alpha = 0;
    uint8_t alpha_reg = 0;
    uint8_t fore_alpha = 0;
    uint32_t pixel_num = (pblender_cfg->img_width * pblender_cfg->img_height);

    CHECK_POINT_NOT_NULL(pblender_cfg);
    CHECK_POINT_NOT_NULL(pdma_cfg);
    CHECK_POINT_NOT_NULL(pout);

    //VIDEO_LOG("pblender_cfg->alpha=0x%x", pblender_cfg->alpha);
    alpha_reg = pblender_cfg->alpha;

    while(pixel_num--)
    {
        switch(pblender_cfg->back_format)
        {
            case BLENDER_BACK_FORMAT_RGB565:
                back_rgb888 = (pback_buf[1] << 8) | (pback_buf[0]);
                back_rgb888 = PIXEL_RGB565_TO_RGB888(back_rgb888);
                pback_buf += 2;
                break;

            case BLENDER_BACK_FORMAT_RGB888:
                back_rgb888 = (pback_buf[2] << 16) | (pback_buf[1] << 8) | (pback_buf[0]);
                pback_buf += 3;
                break;

            case BLENDER_BACK_FORMAT_ARGB8888:
                back_rgb888 = (pback_buf[2] << 16) | (pback_buf[1] << 8) | (pback_buf[0]);
                pback_buf += 4;
                break;

            default:
                ret = FAILURE;
                goto error;
        }

        if(pblender_cfg->blender_mode == 1)  // back + fore
        {
            switch(pblender_cfg->fore_format)
            {
                case BLENDER_FORE_FORMAT_RGB565:
                    fore_rgb888 = (pfore_buf[1] << 8) | (pfore_buf[0]);
                    fore_rgb888 = PIXEL_RGB565_TO_RGB888(fore_rgb888);
                    pfore_buf += 2;
                    break;

                case BLENDER_FORE_FORMAT_RGB888:
                    fore_rgb888 = (pfore_buf[2] << 16) | (pfore_buf[1] << 8) | (pfore_buf[0]);
                    pfore_buf += 3;
                    break;

                case BLENDER_FORE_FORMAT_ARGB8888:
                    fore_rgb888 = (pfore_buf[2] << 16) | (pfore_buf[1] << 8) | (pfore_buf[0]);
                    fore_alpha = pfore_buf[3];
                    pfore_buf += 4;
                    break;

                case BLENDER_FORE_FORMAT_ARGB1555:
                    fore_rgb888 = ((pfore_buf[1] & 0x7C) << 17) | ((pfore_buf[1] & 0x03) << 14)  | ((pfore_buf[0] & 0xE0) << 6) | ((pfore_buf[0] & 0x1F) << 3);
                    fore_alpha = pfore_buf[1] & 0x80;
                    pfore_buf += 2;
                    break;

                case BLENDER_FORE_FORMAT_ARGB4444:
                    fore_rgb888 = ((pfore_buf[1] & 0x0F) << 20) | ((pfore_buf[0] & 0xF0) << 8) | ((pfore_buf[0] & 0x0F) << 4);
                    fore_alpha = pfore_buf[1] & 0xF0;
                    pfore_buf += 2;
                    break;

                case BLENDER_FORE_FORMAT_L8:
                    //fore_rgb888 = pfore_buf[0];
                    fore_rgb888 = (pfore_buf[0] << 16) | (pfore_buf[0] << 8) | (pfore_buf[0]);
                    pfore_buf++;
                    break;

                default:
                    ret = FAILURE;
                    goto error;
            }
            color = fore_rgb888;
        }
        else  // back + color
        {
            //color = (pblender_cfg->color << 16) | (pblender_cfg->color << 8) | pblender_cfg->color;  // ARCS-C
            color = ((pblender_cfg->color & 0xFF) << 16) | (pblender_cfg->color & 0xFF00) | ((pblender_cfg->color & 0xFF0000) >> 16);  // ARCS-D
        }

        switch(pblender_cfg->alpha_mode)
        {
            case BLENDER_ALPHA_MODE_0:      /*!< ALPHA_MODE=0 : alpha = (A * ALPHA_REG) >> 8  */
                if((pblender_cfg->fore_format == BLENDER_FORE_FORMAT_ARGB8888) || \
                        (pblender_cfg->fore_format == BLENDER_FORE_FORMAT_ARGB1555) || \
                        (pblender_cfg->fore_format == BLENDER_FORE_FORMAT_ARGB4444))
                {

                    if(fore_alpha == 0xFF)
                    {
                        if(alpha_reg >= 253) {
                            alpha = 255;
                        } else if(alpha_reg <= 2) {
                            alpha = 0;
                        } else {
                            alpha = alpha_reg;
                        }
                    }
                    else
                    {
                        alpha = (fore_alpha * alpha_reg) >> 8;
                    }
                } else {
                    ret = FAILURE;
                    goto error;
                }
                break;

            case BLENDER_ALPHA_MODE_1:      /*!< ALPHA_MODE=1 : alpha = (mask_buf * ALPHA_REG) >> 8 */
                //VIDEO_LOG("pmask_buf=0x%x, alpha_reg=0x%x", *pmask_buf, alpha_reg);
                //alpha = (*pmask_buf++ * alpha_reg) >> 8;
                //VIDEO_LOG("alpha=0x%x", alpha);
                if(*pmask_buf == 0xFF)
                {
                    if(alpha_reg >= 253) {
                        alpha = 255;
                    } else if(alpha_reg <= 2) {
                        alpha = 0;
                    } else {
                        alpha = alpha_reg;
                    }
                }
                else
                {
                    alpha = (*pmask_buf * alpha_reg) >> 8;
                }
                pmask_buf++;
                break;

            case BLENDER_ALPHA_MODE_2:      /*!< ALPHA_MODE=2 : alpha = ALPHA_REG */
                if(alpha_reg >= 253) {
                    alpha = 255;
                } else if(alpha_reg <= 2) {
                    alpha = 0;
                } else {
                    alpha = alpha_reg;
                }
                break;

            default:
                ret = FAILURE;
                goto error;
        }

        blender_rgb888 = blender_rgb888_pixel_sw(back_rgb888, color, alpha);

        switch(pblender_cfg->back_format)
        {
            case BLENDER_BACK_FORMAT_RGB565:
                blender_rgb888 = PIXEL_RGB888_TO_RGB565(blender_rgb888);
                *pblender_buf++ = blender_rgb888 & 0xFF;
                *pblender_buf++ = (blender_rgb888 >> 8) & 0xFF;
                break;

            case BLENDER_BACK_FORMAT_RGB888:
                *pblender_buf++ = blender_rgb888 & 0xFF;
                *pblender_buf++ = (blender_rgb888 >> 8) & 0xFF;
                *pblender_buf++ = (blender_rgb888 >> 16) & 0xFF;
                break;

            case BLENDER_BACK_FORMAT_ARGB8888:
                *pblender_buf++ = blender_rgb888 & 0xFF;
                *pblender_buf++ = (blender_rgb888 >> 8) & 0xFF;
                *pblender_buf++ = (blender_rgb888 >> 16) & 0xFF;
                *pblender_buf++ = 0xFF;
                break;

            default:
                ret = FAILURE;
                goto error;
        }

    }

    ret = SUCCESS;

error:
    return ret;
}

