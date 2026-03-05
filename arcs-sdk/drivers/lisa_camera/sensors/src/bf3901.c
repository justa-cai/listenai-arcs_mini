#include <stdint.h>
#include <stdio.h>

#include "sensor.h"

#define TAG "bf3901"
#include "lisa_log.h"

#define BF3901_CHIP_ID          0x3901
#define BF3A03_IIC_CLK_FREQ     10000

#define CAMERA_X_MAX (248 - 1)
#define CAMERA_Y_MAX 328

#define BACK_X_LEN              (CAM_FRAME_WIDTH)
#define BACK_Y_LEN              (CAM_FRAME_HEIGHT + 4)

#define CAMERA_LS001_NO_FRAME_FORMAT 1

#define TWI_MAX_COMBINE_REGS    99

struct regval_list {
        uint8_t reg_num;
        uint8_t value;
};

const struct regval_list sensor_default_regs[] = {
{0x15,0x10}, //BSD: Bit[1]: VSYNC = Active low

{0x62,0x81}, // bit[1:0]  01:spi mode   10:2bit mode   11:4bit mode
{0x11,0xb0}, 
{0x1b,0x80},

{0x6b,0x01}, //BSD: no frame HEAD, no frame END, no line HEAD   bit6:CCIR656

{0x08,0xa0},

// {0xb9,0x80}, // test pattern

{0x12,0x01}, //BSD: use LSB, bit[5]: 0=MSB, 1=LSB   bit4: 0=MTK 1=zhan xun
{0x0c,0x40},


{0x06,0x68},
{0x27,0x97},
{0x2b,0x20},
{0x13,0x00},
{0x01,0x0d},
{0x02,0x0d},
// {0x87,0x7f},
// {0x8d,0xff},
{0x20,0x09},
{0x09,0x03}, //BSD: Standby mode, bit[4]: 0=Disable, 1=Enable
{0x33,0x10},
{0x34,0x1d},
{0x35,0x46},
{0x36,0x40},
{0x37,0xa4},
{0x38,0x7c},
{0x65,0x46},
{0x66,0x46},
{0x6e,0x20},
{0x9b,0xa4},
{0x9c,0x7c},
{0xbc,0x0c},
{0xbd,0xa4},
{0xbe,0x7c},
{0x70,0x0f},
{0x71,0x46},
{0x72,0x2f},
{0x73,0x2f},
{0x74,0xa7},
{0x75,0x12},
{0x76,0x90},
{0x77,0xdd},
{0x78,0x4e},
{0x79,0x85},
{0x7a,0x00},
{0x7b,0x55},
{0x7e,0xfa},
{0x7c,0x88},
{0x7d,0xba},
{0x60,0xe7},
{0x61,0xc8},
{0x6d,0x70},
{0x8a,0x11},
{0x8b,0x21},
{0x8e,0x24},
{0x8f,0x31},
{0x94,0x38},
{0x95,0x6e},
{0x96,0x7f},
{0x97,0xf3},
{0x13,0x00},
{0x24,0x50},
{0x97,0x48},
{0x25,0x88},
{0x94,0x42},
{0x95,0xb0},
{0x80,0xd6},
{0x81,0xff},
{0x82,0x18},
{0x83,0x30},
{0x84,0x30},
{0x85,0x40},
{0x86,0x77},
{0x89,0x2d},
{0x8a,0x5e},
{0x8b,0x4c},
{0x98,0x1a},
// {0x39,0x98},
// {0x3f,0x98},
{0x90,0xa0},
{0x91,0xe0},
// {0x40,0x3b},
// {0x41,0x36},
// {0x42,0x2b},
// {0x43,0x1d},
// {0x44,0x1a},
// {0x45,0x14},
// {0x46,0x11},
// {0x47,0x0e},
// {0x48,0x0d},
// {0x49,0x0c},
// {0x4b,0x0b},
// {0x4c,0x09},
// {0x4e,0x09},
// {0x4f,0x08},
// {0x50,0x07},
// { 0x39, 0xc0 },
// { 0x3f, 0x40 },
{0x5a,0x56},
{0x51,0x12},
{0x52,0x0d},
{0x53,0x92},
{0x54,0x7d},
{0x57,0x97},
{0x58,0x43},
{0x5a,0xd6},
{0x51,0x1f},
{0x52,0x0f},
{0x53,0x47},
{0x54,0x20},
{0x57,0x2f},
{0x58,0x24},
{0x5b,0xc2},
{0x5c,0x28},
{0xb0,0xe0},
{0xb3,0x4f},
{0xb4,0xe3},
{0xb1,0xf0},
{0xb2,0xa0},
{0xb4,0x63},
{0xb1,0xd0},
{0xb2,0xc0},
{0x55,0x00},
{0x56,0x60},
{0xa0,0xd0},
{0xa1,0x31},
{0xa6,0x04},
{0xa2,0x0f},
{0xa3,0x2b},
{0xa4,0x0f},
{0xa5,0x2b},
{0xa7,0x9a},
{0xa8,0x1c},
{0xd0,0xb4},
{0xd1,0x00},
{0xd2,0x78},
{0xa9,0x11},
{0xaa,0x16},
{0xab,0x16},
{0xac,0x3c},
{0xad,0xf0},
{0xae,0x57},
{0xc6,0xaa},
{0xc8,0x0d},
{0xc9,0x10},
{0xd3,0x09},
{0xd4,0x24},
{0x6a,0x81},
{0x23,0x33},
{0x69,0x00},
{0x1e,0x39},
{0xee,0x4c},
{0xf1,0x00},
{0x4a,0x0e},
{0xda,0x00},
{0xdb,0xf8}, // 248
{0xdc,0x00},
{0xdd,0x48},
{0xde,0x10}, // 328
{0x17,0x00},
{0x18,0x78}, // 240
{0x19,0x00},
{0x1a,0xa0}, // 320
{0x2b,0x00},
{0x2F,0x04},
{0x16,0xA7},
{0xbb,0x23},
{ 0x13, 0x00 },
{ 0x24, 0xd8 },
{ 0x01, 0x0d },
{ 0x02, 0x0d },
// { 0x87, 0x12 },
{ 0x8c, 0x00 },
{ 0x8d, 0x20 },
{ 0x3e, 0x06 },
{ 0x89, 0x0d },
{ 0x86, 0x40 },
{ 0x8a, 0x18 },
{ 0x01, 0x0d },
{ 0x02, 0x0d },
{ 0x01, 0x0d },
{ 0x02, 0x0d },

{0x0b,0x03},


////////前端////////////////
{0x4a, 0x0e},
{0xda, 0x00},
{0xdb, 96},
{0xdc, 72},
{0xdd, 0x38},
{0xde, 0x10},
///////////////后端/////////////
{0x17, 0x00},
{0x18, 96 >> 1},
{0x19, 0x00},
{0x1a, 240 >> 1},

/* 0x92[L]/0x93[H]  0xe3[L]/0xe4[H]通过插入dummy line改变帧率 */
// {0x92, 0x8},
// {0xe3, 0x8},
// {0x0b,0x01},
// {0xb9,0x00},

//gamma
{0x39,0x98},
{0x3f,0x98},
{0X40,0x18},
{0X41,0x25},
{0X42,0x22},
{0X43,0x1f},
{0X44,0x1c},
{0X45,0x1a},
{0X46,0x17},
{0X47,0x15},
{0X48,0x11},
{0X49,0x0e},
{0X4b,0x0b},
{0X4c,0x0a},
{0X4e,0x09},
{0X4f,0x08},
{0X50,0x06},

//praw ISP:Denoise LSC gamma
{0x12,0x05},
{0x0c,0xc0},
//denoise
// {0x70,0x80},
// {0x72,0x0f},
// {0x73,0x0f},
{0x87,0x0f},
{0x35,0x66},
{0x65,0x66},
{0x66,0x66},

{0x28,0x00},
{0xd5,0x00},
{0xd6,0x00},
{0xd7,0x00},
{0xd8,0x02},
{0x61,0xc8},
{0x0d,0x1d},
{0x00,0x1c},
{0x61,0x88},
{0x0d,0x2a},
{0x00,0x2a},

{0xff, 0xff},
};

static int get_reg(sensor_t *sensor, int reg, int mask)
{
    int ret = CAMERA_READ_REG8(sensor->slv_addr, reg & 0xFF);
    if(ret > 0){
        ret &= mask;
    }
    return ret;
}

static int set_reg(sensor_t *sensor, int reg, int mask, int value)
{
    int ret = 0;
    ret = CAMERA_READ_REG8(sensor->slv_addr, reg & 0xFF);
    if(ret < 0){
        return ret;
    }
    value = (ret & ~mask) | (value & mask);
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, reg & 0xFF, value);
    return ret;
}


static int get_reg_bits(sensor_t *sensor, uint8_t reg, uint8_t offset, int len)
{
    int ret = 0;
    ret = CAMERA_READ_REG8(sensor->slv_addr, reg);
    if (ret < 0) {
        return ret;
    }
    uint8_t mask = ((1 << len) - 1) << offset;
    return (ret & mask) >> offset;
}

static int set_reg_bits(sensor_t *sensor, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    ret = CAMERA_READ_REG8(sensor->slv_addr, reg);
    if(ret < 0){
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, reg & 0xFF, value);
    return ret;
}

static int bf3901_write_array(sensor_t *sensor, const struct regval_list *vals)
{
    int ret = 0;
#if 0
    while ( ((vals->reg_num != 0xff) || (vals->value != 0xff)) && (0 == ret) ) {
        ret = CAMERA_WRITE_REG8(sensor->slv_addr, vals->reg_num, vals->value);
        vals++;
    }
#else
    uint8_t regbuf[1 + TWI_MAX_COMBINE_REGS] = { 0 };  // first one for register, left for values
    uint8_t regcnt = 0;
    while (((vals->reg_num != 0xff) || (vals->value != 0xff)) && (0 == ret)) {
        if (0 < regcnt && regcnt < TWI_MAX_COMBINE_REGS && regbuf[0] + regcnt == vals->reg_num)
            regbuf[++regcnt] = vals->value;
        else {
            if (regcnt > 0) ret = CAMERA_WRITE_RAW8(sensor->slv_addr, regbuf, 1 + regcnt);
            regbuf[0] = vals->reg_num;
            regbuf[1] = vals->value;
            regcnt = 1;
        }
        vals++;
    }
    if (regcnt > 0)
        ret = CAMERA_WRITE_RAW8(sensor->slv_addr, regbuf, 1 + regcnt);
#endif
    return ret;
}


static pixformat_t get_pixformat(sensor_t *sensor)
{
    int reg_val = CAMERA_READ_REG8(sensor->slv_addr, 0x12);
    if (reg_val < 0) {
        return PIXFORMAT_INVALID;
    }

    uint8_t format_bits = ((reg_val >> 2) & 0x01) << 1 | (reg_val & 0x01);

    switch (format_bits) {
    case 0b00: // YUV422
        return PIXFORMAT_YUV422;
    case 0b01: // RAW
    case 0b11: // Processed RAW
        return PIXFORMAT_GRAYSCALE;
    case 0b10: // RGB565
        return PIXFORMAT_RGB565;
    default:
        return PIXFORMAT_INVALID;
    }
}


static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    *w = CAMERA_READ_REG8(sensor->slv_addr, 0x18) << 1;
    *h = CAMERA_READ_REG8(sensor->slv_addr, 0x1a) << 1;

    return 0;
}


static int reset(sensor_t *sensor)
{
    bf3901_write_array(sensor, sensor_default_regs);

    CAMERA_DELAY_MS(5);

    return 0;
}

static int start(sensor_t *sensor)
{
    set_reg_bits(sensor, 0x09, 4, 1, 0x00);
    return 0;
}

static int stop(sensor_t *sensor)
{
    set_reg_bits(sensor, 0x09, 4, 1, 0x01);
    return 0;
}

static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret=0;
    uint8_t reg_val = CAMERA_READ_REG8(sensor->slv_addr, 0x12);

    // mask for bit0 and bit2
    uint8_t mask = (1 << 2) | (1 << 0);
    uint8_t value = 0;

    switch (pixformat) {
    case PIXFORMAT_YUV422:
        value = 0x00; // bit2=0, bit0=0
        break;
    case PIXFORMAT_GRAYSCALE:
        value = 0x01; // bit2=0, bit0=1
        break;
    case PIXFORMAT_RGB565:
        value = 0x04; // bit2=1, bit0=0
        break;
    case PIXFORMAT_RAW:
        value = 0x05; // bit2=1, bit0=1
        break;
    default:
        return -1;
    }

    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x12, (reg_val & ~mask) | value);
    if (ret == 0) {
        sensor->pixformat = pixformat;
    }

    // Delay
    CAMERA_DELAY_MS(30);

    return ret;
}

static int set_window(sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    uint8_t reg_da = 0; /* x_win_start */
    uint8_t reg_db = 0; /* x_win_end */
    uint8_t reg_dc = 0; /* y_win_start, low 8 bit */
    uint8_t reg_dd = 0; /* y_win_end, low 8 bit */
    uint8_t reg_de = 0; /* bit[0]: y start high 1 bit, bit[5]:y end high 1 bit,  */
    int ret=0;

    int16_t x_end = 0;
    int16_t y_end = 0;
    
    x = x < 0 ? 0 : x;
    x_end = x + w;
    x_end = x_end > CAMERA_X_MAX ? CAMERA_X_MAX : x_end;
    x = x_end - w;

    y = y < 0 ? 0 : y;
    y_end = y + h;
    y_end = y_end > CAMERA_Y_MAX ? CAMERA_Y_MAX : y_end;
    y = y_end - h;

    reg_da = x;
    reg_db = x_end;

    reg_dc = y & 0xff;
    reg_dd = y_end & 0xff;

    /**
     * Note that the high bit of y_end
     * is register 0xde bit4 instead of bit5.
     * There may be an error in the data sheet.
     */
    reg_de = ((y_end > (int16_t)255) << 4) | ((y) > (int16_t)255);

    LOGI("x:%d, y:%d, w:%d, h:%d\r\n", x, y, w, h);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xda, reg_da);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xdb, reg_db);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xdc, reg_dc);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xdd, reg_dd);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xde, reg_de);

    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x17, 0x00);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x18, w>>1);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x19, 0x00);
    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0x1a, h>>1);

    return ret;
}

static int set_colorbar(sensor_t *sensor, int value)
{
    int ret=0;
    sensor->status.colorbar = value;

    ret |= CAMERA_WRITE_REG8(sensor->slv_addr, 0xb9, value);

    return ret;
}

static int set_whitebal(sensor_t *sensor, int enable)
{
    if(set_reg_bits(sensor, 0x13, 1, 1, enable) >= 0){
        sensor->status.awb = !!enable;
    }
    return sensor->status.awb;
}

static int set_gain_ctrl(sensor_t *sensor, int enable)
{
    if(set_reg_bits(sensor, 0x13, 2, 1, enable) >= 0){
        sensor->status.agc = !!enable;
    }
    return sensor->status.agc;
}


static int set_exposure_ctrl(sensor_t *sensor, int enable)
{
    if(set_reg_bits(sensor, 0x13, 0, 1, enable) >= 0){
        sensor->status.aec = !!enable;
    }
    return sensor->status.aec;
}

static int set_hmirror(sensor_t *sensor, int enable)
{
    if(set_reg_bits(sensor, 0x1e, 5, 1, enable) >= 0){
        sensor->status.hmirror = !!enable;
    }
    return sensor->status.hmirror;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    if(set_reg_bits(sensor, 0x1e, 4, 1, enable) >= 0){
        sensor->status.vflip = !!enable;
    }
    return sensor->status.vflip;
}

static int set_raw_gma_dsp(sensor_t *sensor, int enable)
{
    int ret = 0;
    ret = set_reg_bits(sensor, 0xf1, 1, 1, !enable);
    if (ret == 0) {
        CLOGD("Set raw_gma to: %d", !enable);
        sensor->status.raw_gma = !enable;
    }
    return ret;
}


static int set_lenc_dsp(sensor_t *sensor, int enable)
{
    int ret = 0;
    ret = set_reg_bits(sensor, 0xf1, 0, 1, !enable);
    if (ret == 0) {
        CLOGD("Set lenc to: %d", !enable);
        sensor->status.lenc = !enable;
    }
    return ret;
}

static int set_agc_gain(sensor_t *sensor, int option)
{
    int ret = 0;
    ret = set_reg_bits(sensor, 0x13, 4, 1, !!option);
    if (ret == 0) {
        CLOGD("Set gain to: %d", !!option);
        sensor->status.agc_gain = !!option;
    }
    return ret;
}

static int set_awb_gain_dsp(sensor_t *sensor, int value)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0xa6, value);
    if (ret == 0) {
        CLOGD("Set awb gain threthold to: %d", value);
        sensor->status.awb_gain = value;
    }
    return ret;
}

static int set_brightness(sensor_t *sensor, int level)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x55, level);
    if (ret == 0) {
        CLOGD("Set brightness to: %d", level);
        sensor->status.brightness = level;
    }
    return ret;
}

static int set_contrast(sensor_t *sensor, int level)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x56, level);
    if (ret == 0) {
        CLOGD("Set contrast to: %d", level);
        sensor->status.contrast = level;
    }
    return ret;
}

static int set_sharpness(sensor_t *sensor, int level)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x70, level);
    if (ret == 0) {
        CLOGD("Set sharpness to: %d", level);
        sensor->status.sharpness = level;
    }
    return ret;
}

static int set_gainceiling(sensor_t *sensor, gainceiling_t val)
{
    int ret = 0;
    ret = CAMERA_WRITE_REG8(sensor->slv_addr, 0x87, val);
    if (ret == 0) {
        CLOGD("Set gain ceiling to: %d", val);
        sensor->status.gainceiling = val;
    }
    return ret;
}

static int init_status(sensor_t *sensor)
{
    sensor->status.brightness = CAMERA_READ_REG8(sensor->slv_addr, 0x55);
    sensor->status.contrast = CAMERA_READ_REG8(sensor->slv_addr, 0x56);
    sensor->status.saturation = 0;
    sensor->status.ae_level = 0;
    
    sensor->status.gainceiling = CAMERA_READ_REG8(sensor->slv_addr, 0x87);
    sensor->status.awb = get_reg_bits(sensor, 0x13, 1, 1);
    sensor->status.awb_gain = CAMERA_READ_REG8(sensor->slv_addr, 0xa6);
    sensor->status.aec = get_reg_bits(sensor, 0x13, 0, 1);

    sensor->status.agc = get_reg_bits(sensor, 0x13, 2, 1);
    
    sensor->status.raw_gma = get_reg_bits(sensor, 0xf1, 1, 1);
    sensor->status.lenc = get_reg_bits(sensor, 0xf1, 0, 1);
    sensor->status.hmirror = get_reg_bits(sensor, 0x1e, 5, 1);
    sensor->status.vflip = get_reg_bits(sensor, 0x1e, 4, 1);
    
    sensor->status.colorbar = CAMERA_READ_REG8(sensor->slv_addr, 0xb9);
    sensor->status.sharpness = CAMERA_READ_REG8(sensor->slv_addr, 0x70);

    return 0;
}

static int set_dummy(sensor_t *sensor, int val){ return -1; }
static int set_res_raw(sensor_t *sensor, int startX, int startY, int endX, int endY, int offsetX, int offsetY, int totalX, int totalY, int outputX, int outputY, bool scale, bool binning){return -1;}
static int _set_pll(sensor_t *sensor, int bypass, int multiplier, int sys_div, int root_2x, int pre_div, int seld5, int pclk_manual, int pclk_div){return -1;}

static int set_xclk(sensor_t *sensor, int timer, int xclk)
{
    int ret = 0;
    sensor->xclk_freq_hz = xclk * 1000000U;
    //ret = xclk_timer_conf(timer, sensor->xclk_freq_hz);
    return ret;
}

int bf3901_detect(int slv_addr, sensor_id_t *id)
{
    if (BF3901_SCCB_ADDR == slv_addr) {
        uint16_t chip_id = 0;
        unsigned char val = 0;
        val = CAMERA_READ_REG8(slv_addr, 0xFC);
        chip_id |= (val << 8);
        val = CAMERA_READ_REG8(slv_addr, 0xfd);
        chip_id |= (val);
        if (BF3901_PID == chip_id) {
            id->PID = chip_id;
            return chip_id;
        }
        CLOGI("Mismatch PID=0x%x", chip_id);
    }
    return 0;
}

int bf3901_init(sensor_t *sensor)
{
    // Set function pointers
    sensor->reset = reset;
    sensor->start = start;
    sensor->stop = stop;
    sensor->init_status = init_status;
    sensor->set_pixformat = set_pixformat;
    sensor->set_brightness = set_brightness;
    sensor->set_contrast = set_contrast;

    sensor->set_colorbar = set_colorbar;

    sensor->set_gain_ctrl = set_gain_ctrl;
    sensor->set_exposure_ctrl = set_exposure_ctrl;
    sensor->set_hmirror = set_hmirror;
    sensor->set_vflip = set_vflip;
    sensor->set_window = set_window;
    sensor->get_window = get_window;

    sensor->set_whitebal = set_whitebal;

    sensor->set_awb_gain = set_awb_gain_dsp;
    sensor->set_agc_gain = set_agc_gain;
    
    sensor->set_raw_gma = set_raw_gma_dsp;
    sensor->set_lenc = set_lenc_dsp;

    sensor->set_sharpness = set_sharpness;

    sensor->get_pixformat = get_pixformat;
    //not supported
    sensor->set_saturation= set_dummy;
    sensor->set_denoise = set_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_special_effect = set_dummy;
    sensor->set_wb_mode = set_dummy;
    sensor->set_ae_level = set_dummy;
    sensor->set_gainceiling = set_gainceiling;


    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_res_raw = set_res_raw;
    sensor->set_pll = _set_pll;
    sensor->set_xclk = set_xclk;
    
    CLOGD("BF3901 Attached\r\n");

    return 0;
}
