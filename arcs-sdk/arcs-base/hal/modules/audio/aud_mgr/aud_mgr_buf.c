/**
 ****************************************************************************************
 * @file aud_mgr.c
 *
 * @brief  audio manager source
 *
 * Copyright (C) Listenai 2023
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup AUDIO
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "aud_mgr_buf.h"
#include <string.h>            // For memset

/*
 * MACROS
 ****************************************************************************************
 */

/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
uint8_t aud_buf_init(aud_buf_info_t *aud_buf, uint16_t size)
{
    uint8_t status = AUD_ERROR_NO;
    if(aud_buf)
    {
        aud_buf->buf_size = size;
        aud_buf->buf = AUD_MALLOC(aud_buf->buf_size);
        if(aud_buf->buf == NULL)
        {
            status = AUD_ERROR_BUF_NO_RESOURCE;
        }
        else
        {
            memset(aud_buf->buf, 0, aud_buf->buf_size);
        }
        aud_buf->read_pos    = 0;
        aud_buf->read_ring   = 0;
        aud_buf->write_pos   = 0;
        aud_buf->write_ring  = 0;
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    return status;
}

uint8_t aud_buf_deinit(aud_buf_info_t *aud_buf)
{
    uint8_t status = AUD_ERROR_NO;
    if(aud_buf)
    {
        if(aud_buf->buf != NULL)
        {
            AUD_FREE(aud_buf->buf);
            aud_buf->buf = NULL;
        }
        aud_buf->read_pos    = 0;
        aud_buf->read_ring   = 0;
        aud_buf->write_pos   = 0;
        aud_buf->write_ring  = 0;
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    
    return status;
}

void aud_buf_reset(aud_buf_info_t *aud_buf)
{
    if(aud_buf)
    {
        aud_buf->read_pos    = 0;
        aud_buf->read_ring   = 0;
        aud_buf->write_pos   = 0;
        aud_buf->write_ring  = 0;
    }
}

uint16_t aud_buf_data_len(aud_buf_info_t *aud_buf)
{
    uint16_t data_len = 0;
    if(aud_buf)
    {
        if(aud_buf->write_pos > aud_buf->read_pos)
        {
            data_len = aud_buf->write_pos - aud_buf->read_pos;
        }
        else if(aud_buf->write_pos < aud_buf->read_pos)
        {
            data_len = aud_buf->buf_size - (aud_buf->read_pos - aud_buf->write_pos);
        }
        else/// read_pos is equal write_pos
        {
            if(aud_buf->read_ring == aud_buf->write_ring)
            {
                data_len = 0;
            }
            else
            {
                data_len = aud_buf->buf_size;
            }
        }
    }
    return data_len;
}

uint16_t aud_buf_free_len(aud_buf_info_t *aud_buf)
{
    uint16_t data_len = aud_buf->buf_size - aud_buf_data_len(aud_buf);
    return data_len;
}

uint16_t aud_buf_data_in(aud_buf_info_t *aud_buf, uint16_t data_len, uint8_t *data_in)
{
    if(aud_buf && data_in)
    {
        uint16_t free_len = aud_buf->buf_size - aud_buf_data_len(aud_buf);

        if(data_len > free_len)
        {
            return 0;
        }

        if((aud_buf->buf_size - aud_buf->write_pos) > data_len)
        {
            memcpy(&aud_buf->buf[aud_buf->write_pos], data_in, data_len);
            //CLOGD("aud buf in, header:0x%x", aud_buf->buf[aud_buf->write_pos]);
            aud_buf->write_pos += data_len;
            return data_len;
        }
        else
        {
            uint16_t head_len = aud_buf->buf_size - aud_buf->write_pos;
            uint16_t tail_len = data_len - head_len;
            memcpy(&aud_buf->buf[aud_buf->write_pos], data_in, head_len);
            memcpy(&aud_buf->buf[0], data_in + head_len, tail_len);
            //CLOGD("aud buf in, header:0x%x", aud_buf->buf[aud_buf->write_pos]);
            aud_buf->write_pos = tail_len;
            aud_buf->write_ring = ~aud_buf->write_ring;
        }
        return data_len;
    }
    else
    {
        return 0;
    }
}
uint16_t aud_buf_data_out(aud_buf_info_t *aud_buf, uint16_t data_len, uint8_t *data_out)
{
    if(aud_buf && data_out)
    {
        uint16_t left_len = aud_buf_data_len(aud_buf);
        
        if(data_len > left_len)
        {
            return 0;
        }
        if((aud_buf->buf_size - aud_buf->read_pos) > data_len)
        {
            memcpy(data_out, &aud_buf->buf[aud_buf->read_pos], data_len);
            //CLOGD("aud buf out, header:0x%x", data_out);
            aud_buf->read_pos += data_len;
        }
        else
        {
            uint16_t head_len = aud_buf->buf_size - aud_buf->read_pos;
            uint16_t tail_len = data_len - head_len;
            memcpy(data_out, &aud_buf->buf[aud_buf->read_pos], head_len);
            memcpy(data_out + head_len, &aud_buf->buf[0], tail_len);
            //CLOGD("aud buf out, header:0x%x", data_out);
            aud_buf->read_pos = tail_len;
            aud_buf->read_ring = ~aud_buf->read_ring;
        }
        return data_len;
    }
    else
    {
        return 0;
    }
}
uint8_t *aud_buf_data_in_ptr(aud_buf_info_t *aud_buf)
{
    return &aud_buf->buf[aud_buf->write_pos];
}
uint16_t aud_buf_data_async_in(aud_buf_info_t *aud_buf, uint16_t data_len)
{
    if(aud_buf)
    {
        uint16_t free_len = aud_buf->buf_size - aud_buf_data_len(aud_buf);

        if(data_len > free_len)
        {
            return 0;
        }

        if((aud_buf->buf_size - aud_buf->write_pos) > data_len)
        {
            //CLOGD("aud buf in, header:0x%x", aud_buf->buf[aud_buf->write_pos]);
            aud_buf->write_pos += data_len;
        }
        else if((aud_buf->buf_size - aud_buf->write_pos) == data_len)
        {
            aud_buf->write_pos = 0;
            aud_buf->write_ring = ~aud_buf->write_ring;
        }
        else//NOTE:to improve efficiency, async data write or read cannot cross boundaries.
        {
            CLOGD("aud buf async in error, buf:%d,pos:%d %d, len:%d", aud_buf->buf_size, aud_buf->read_pos, aud_buf->write_pos, data_len);
            data_len = 0;
            aud_buf_reset(aud_buf);
        }
        return data_len;
    }
    else
    {
        return 0;
    }
}

uint8_t *aud_buf_data_out_ptr(aud_buf_info_t *aud_buf)
{
    return &aud_buf->buf[aud_buf->read_pos];
}

uint16_t aud_buf_data_async_out(aud_buf_info_t *aud_buf, uint16_t data_len)
{
    if(aud_buf)
    {
        uint16_t left_len = aud_buf_data_len(aud_buf);
        
        if(data_len > left_len)
        {
            return 0;
        }
        if((aud_buf->buf_size - aud_buf->read_pos) > data_len)
        {
            //CLOGD("aud buf out, header:0x%x", data_out);
            aud_buf->read_pos += data_len;
        }
        else if((aud_buf->buf_size - aud_buf->read_pos) == data_len)
        {
            //CLOGD("aud buf out, header:0x%x", data_out);
            aud_buf->read_pos = 0;
            aud_buf->read_ring = ~aud_buf->read_ring;
        }
        else//NOTE:to improve efficiency, async data write or read cannot cross boundaries.
        {
            CLOGD("aud buf async out error, buf:%d,pos:%d %d, len:%d", aud_buf->buf_size, aud_buf->read_pos, aud_buf->write_pos, data_len);
            data_len = 0;
            aud_buf_reset(aud_buf);
        }
        return data_len;
    }
    else
    {
        return 0;
    }
}

uint8_t aud_pro_buf_init(aud_pro_buf_info_t *aud_pro_buf, uint16_t dec_in_size, uint16_t dec_out_size, uint16_t enc_in_size, uint16_t enc_out_size)
{
    uint8_t status = AUD_ERROR_NO;
    if(aud_pro_buf)
    {
        if(dec_in_size > 0)
        {
            aud_pro_buf->dec_in_buf = AUD_MALLOC(dec_in_size);
            memset(aud_pro_buf->dec_in_buf, 0, dec_in_size);
        }
        /// point to dec out buf, so not malloc there.
        //if(dec_out_size >= 0)
        //{
        //    aud_pro_buf->dec_out_buf = AUD_MALLOC(dec_out_size);
        //    memset(aud_pro_buf->dec_out_buf, 0, dec_out_size);
        //}

        if(enc_in_size > 0)
        {
            aud_pro_buf->enc_in_buf = AUD_MALLOC(enc_in_size);
            memset(aud_pro_buf->enc_in_buf, 0, enc_in_size);
        }

        if(enc_out_size > 0)
        {
            aud_pro_buf->enc_out_buf = AUD_MALLOC(enc_out_size);
            memset(aud_pro_buf->enc_out_buf, 0, enc_out_size);
        }
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    return status;
}
uint8_t aud_pro_buf_deinit(aud_pro_buf_info_t *aud_pro_buf)
{
    uint8_t status = AUD_ERROR_NO;
    CLOGD("aud_pro_buf_deinit");
    if(aud_pro_buf)
    {
        if(aud_pro_buf->dec_in_buf)
        {
            AUD_FREE(aud_pro_buf->dec_in_buf);
            aud_pro_buf->dec_in_buf = NULL;
        }
        /// point to dec out buf, so not free there.
        //if(aud_pro_buf->dec_out_buf)
        //{
        //    AUD_FREE(aud_pro_buf->dec_out_buf);
        //    aud_pro_buf->dec_out_buf = NULL;
        //}
        if(aud_pro_buf->enc_in_buf)
        {
            AUD_FREE(aud_pro_buf->enc_in_buf);
            aud_pro_buf->enc_in_buf = NULL;
        }
        if(aud_pro_buf->enc_out_buf)
        {
            AUD_FREE(aud_pro_buf->enc_out_buf);
            aud_pro_buf->enc_out_buf = NULL;
        }
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    return status;
}


#if 0
uint8_t aud_out_buf_init(aud_out_buf_info_t *aud_buf, uint16_t size)
{
    uint8_t status = AUD_ERROR_NO;
    if(aud_buf)
    {
        aud_buf->buf_size = size;
        aud_buf->buf = AUD_MALLOC(aud_buf->buf_size);
        if(aud_buf->buf == NULL)
        {
            status = AUD_ERROR_BUF_NO_RESOURCE;
        }
        aud_buf->read_pos    = 0;
        aud_buf->write_pos   = 0;
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    return status;
}

uint8_t aud_out_buf_deinit(aud_out_buf_info_t *aud_buf)
{
    uint8_t status = AUD_ERROR_NO;
    if(aud_buf)
    {
        if(aud_buf->buf != NULL)
        {
            AUD_FREE(aud_buf->buf);
            aud_buf->buf = NULL;
        }
        aud_buf->read_pos    = 0;
        aud_buf->write_pos   = 0;
    }
    else
    {
        status = AUD_ERROR_PARAM_UNAVAILE;
    }
    
    return status;
}
#endif
/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */


/// @} AUDIO



