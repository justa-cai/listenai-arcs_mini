#include "sw_i2c.h"
#include "config.h"

#define I2C_NUM_MAX         2
#define I2C_DELAY_US        5

sw_i2c_port_callback_t sw_i2c_port_callback[I2C_NUM_MAX] = {0};

#define SW_I2C_ADDR_READ(addr_7bit)         (((addr_7bit) << 1) | 0x01)
#define SW_I2C_ADDR_WRITE(addr_7bit)        (((addr_7bit) << 1) & 0xFE)

#define SW_I2C_SCL_Set(_i2c_index)          sw_i2c_port_callback[_i2c_index].scl_set()
#define SW_I2C_SCL_Clr(_i2c_index)          sw_i2c_port_callback[_i2c_index].scl_clr()

#define SW_I2C_SDA_Set(_i2c_index)          sw_i2c_port_callback[_i2c_index].sda_set()
#define SW_I2C_SDA_Clr(_i2c_index)          sw_i2c_port_callback[_i2c_index].sda_clr()

#define SW_I2C_SDA_Get(_i2c_index)          sw_i2c_port_callback[_i2c_index].sda_get()

#define SW_I2C_SDA_DirOut(_i2c_index)       sw_i2c_port_callback[_i2c_index].sda_dirout()
#define SW_I2C_SDA_DirIn(_i2c_index)        sw_i2c_port_callback[_i2c_index].sda_dirin()

//#define SW_I2C_DELAY_MS(_nms)               do { \
//            if(sw_i2c_port_callback[0].delay_ms) { sw_i2c_port_callback[0].delay_ms(_nms); } \
//            else if(sw_i2c_port_callback[0].delay_ms) { sw_i2c_port_callback[0].delay_ms(_nms); } \
//            else {} \
//            } while(0)
//
//#define SW_I2C_DELAY_US(_nus)               do { \
//            if(sw_i2c_port_callback[0].delay_us) { sw_i2c_port_callback[0].delay_us(_nus); } \
//            else if(sw_i2c_port_callback[0].delay_us) { sw_i2c_port_callback[0].delay_us(_nus); } \
//            else {} \
//            } while(0)
//
//#define SW_I2C_LOG(fmt, ...)                do { \
//            if(sw_i2c_port_callback[0].log) { sw_i2c_port_callback[0].log(fmt, ##__VA_ARGS__); } \
//            else if(sw_i2c_port_callback[0].log) { sw_i2c_port_callback[0].log(fmt, ##__VA_ARGS__); } \
//            else {} \
//            } while(0)
#define SW_I2C_DELAY_MS(_nms)               DELAY_MS(_nms)
#define SW_I2C_DELAY_US(_nus)               DELAY_US(_nus)
#define SW_I2C_LOG(fmt, ...)                VIDEO_LOG(fmt, ##__VA_ARGS__)

#define SW_I2C_DELAY()                      SW_I2C_DELAY_US(I2C_DELAY_US)


int sw_i2c_init(uint8_t i2c_index, sw_i2c_port_callback_t *callback)
{
    if((NULL == callback) || (i2c_index >= I2C_NUM_MAX)) {
        return -1;
    }

    memcpy(&sw_i2c_port_callback[i2c_index], callback, sizeof(sw_i2c_port_callback_t));

    SW_I2C_LOG("[%s:%d] i2c_index=%d", __func__, __LINE__, i2c_index);
    sw_i2c_port_callback[i2c_index].gpio_init();

    return 0;
}


/*******************************************************************************************************/
static void iic_start(uint8_t i2c_index)
{
    SW_I2C_SDA_Set(i2c_index);
    SW_I2C_SCL_Set(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SDA_Clr(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Clr(i2c_index);
    SW_I2C_DELAY();
}

static void iic_stop(uint8_t i2c_index)
{
    SW_I2C_SDA_Clr(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Set(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SDA_Set(i2c_index);
    SW_I2C_DELAY();
}

static uint8_t iic_wait_ack(uint8_t i2c_index)
{
    uint8_t timeout = 0;
    uint8_t rack = 0;

    SW_I2C_SDA_Set(i2c_index);
    SW_I2C_DELAY();

    SW_I2C_SDA_DirIn(i2c_index);
    while (SW_I2C_SDA_Get(i2c_index))
    {
        timeout++;
        SW_I2C_DELAY_US(1);

        if (timeout > 200)
        {
            SW_I2C_SDA_DirOut(i2c_index);
            iic_stop(i2c_index);
            rack = 1;
            return rack;
        }
    }
    SW_I2C_SDA_DirOut(i2c_index);

    SW_I2C_SCL_Set(i2c_index);
    SW_I2C_DELAY();

    SW_I2C_SCL_Clr(i2c_index);
    SW_I2C_DELAY();
    return rack;
}

static void iic_ack(uint8_t i2c_index)
{
    SW_I2C_SDA_Clr(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Set(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Clr(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SDA_Set(i2c_index);
    SW_I2C_DELAY();
}

static void iic_nack(uint8_t i2c_index)
{
    SW_I2C_SDA_Set(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Set(i2c_index);
    SW_I2C_DELAY();
    SW_I2C_SCL_Clr(i2c_index);
    SW_I2C_DELAY();
}

static void iic_send_byte(uint8_t i2c_index, uint8_t data)
{
    uint8_t t;

    for (t = 0; t < 8; t++)
    {
        if (data & 0x80) {
            SW_I2C_SDA_Set(i2c_index);
        } else {
            SW_I2C_SDA_Clr(i2c_index);
        }
        SW_I2C_DELAY();
        SW_I2C_SCL_Set(i2c_index);
        SW_I2C_DELAY();
        SW_I2C_SCL_Clr(i2c_index);
        data <<= 1;
    }
    SW_I2C_SDA_Set(i2c_index);
}

static uint8_t iic_read_byte(uint8_t i2c_index, uint8_t ack)
{
    uint8_t i, receive = 0;

    SW_I2C_SDA_DirIn(i2c_index);
    for (i = 0; i < 8; i++ )
    {
        receive <<= 1;
        SW_I2C_SCL_Set(i2c_index);
        SW_I2C_DELAY();

        if (SW_I2C_SDA_Get(i2c_index))
        {
            receive++;
        }

        SW_I2C_SCL_Clr(i2c_index);
        SW_I2C_DELAY();
    }
    SW_I2C_SDA_DirOut(i2c_index);

    if (!ack)
    {
        iic_nack(i2c_index);
    }
    else
    {
        iic_ack(i2c_index);
    }

    return receive;
}


uint8_t sw_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg)
{
    uint8_t value = 0;

    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_WRITE(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, reg);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_READ(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    value = iic_read_byte(i2c_index, 0);

    iic_stop(i2c_index);

    return value;
}


int sw_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value)
{
    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_WRITE(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, reg);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, value);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_stop(i2c_index);

    return 0;
}


uint8_t sw_i2c_read_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg)
{
    uint8_t value = 0;

    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_WRITE(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, (reg >> 8) & 0xff);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }
    iic_send_byte(i2c_index, reg & 0xff);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_READ(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }
    value = iic_read_byte(i2c_index, 0);

    iic_stop(i2c_index);

    return value;
}


int sw_i2c_write_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg, uint8_t value)
{
    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_WRITE(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, (reg >> 8) & 0xff);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }
    iic_send_byte(i2c_index, reg & 0xff);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_send_byte(i2c_index, value);
    if(iic_wait_ack(i2c_index))
    {
        SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_stop(i2c_index);

    return 0;
}


uint8_t sw_i2c_detect(uint8_t i2c_index, uint16_t addr)
{
    iic_start(i2c_index);

    iic_send_byte(i2c_index, SW_I2C_ADDR_WRITE(addr & 0xFF));
    if(iic_wait_ack(i2c_index))
    {
        //SW_I2C_LOG("[%s:%d] no ack", __func__, __LINE__);
        return 0;
    }

    iic_stop(i2c_index);

    return 1;
}


int sw_i2c_detect_all(uint8_t i2c_index)
{
    int ret = 0;

    uint8_t addr_7bit = 0;

    SW_I2C_LOG("[%s:%d]", __func__, __LINE__);

    for(addr_7bit = 0; addr_7bit <= 0x7F; addr_7bit++)
    {
        if(sw_i2c_detect(i2c_index, addr_7bit)) {
            SW_I2C_LOG("i2c_detect addr_7bit=0x%x", addr_7bit);
            ret++;
        }
        SW_I2C_DELAY_MS(1);
    }

    return ret;
}


