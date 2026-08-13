/*
 * SC030IOT driver.
 *
 * Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG  "sc030: "
#include "sensor.h"
#include "sc030iot.h"
#include "sc030iot_settings.h"




#define SC030_SENSOR_ID_HIGH_REG    0XF7
#define SC030_SENSOR_ID_LOW_REG     0XF8
#define SC030_MAX_FRAME_WIDTH       (640)
#define SC030_MAX_FRAME_HIGH        (480)

// sc030 use "i2c paging mode", so the high byte of the register needs to be written to the 0xf0 reg.
// For more information please refer to the Technical Reference Manual.
static int get_reg(sensor_t *sensor, int reg, int reg_value_mask)
{
    int ret = 0;
    uint8_t reg_high = (reg>>8) & 0xFF;
    uint8_t reg_low = reg & 0xFF;

    if(CAMERA_WRITE_REG8(sensor->slv_addr, 0xf0, reg_high)) {
        return -1;
    }

    ret = CAMERA_READ_REG8(sensor->slv_addr, reg_low);
    if(ret > 0){
        ret &= reg_value_mask;
    }
    return ret;
}

// sc030 use "i2c paging mode", so the high byte of the register needs to be written to the 0xf0 reg.
// For more information please refer to the Technical Reference Manual.
static int set_reg(sensor_t *sensor, int reg, int mask, int value)
{
    int ret = 0;
    uint8_t reg_high = (reg>>8) & 0xFF;
    uint8_t reg_low = reg & 0xFF;

    if(CAMERA_WRITE_REG8(sensor->slv_addr, 0xf0, reg_high)) {
        return -1;
    }

    ret = CAMERA_WRITE_REG8(sensor->slv_addr, reg_low, value & 0xFF);
    return ret;
}

#define SC030IOT_I2C_BURST_MAX    (64U)

/**
 * @brief 批量写入 SC030 寄存器表(支持连续地址突发写)。
 *
 * @details
 * SC030 使用 I2C 分页模式,寄存器表以 @c {reg, val} 形式给出,其中
 * @c 0xf0 为分页寄存器,用于切换寄存器页。本函数在遍历寄存器表时,
 * 将地址连续的表项合并成一次 I2C 突发写事务(@c [reg, v0, v1, ...],
 * 依赖 sensor 的写地址自增特性),以保证厂方期望原子写入的连续寄存器块
 * (如 PLL/时序耦合寄存器)在一次事务内完成,避免半更新态生效导致输出
 * 时序不稳。
 *
 * 合并/断开规则:
 * - 遇到分页寄存器 @c 0xf0 时强制断开当前突发,并单独成一次事务写入,
 *   切页后延时 3ms 等待分页生效,再连续写该页数据,确保分页写始终干净、
 *   不依赖地址连续性的运气。
 * - 当前地址与上一项地址不连续,或缓冲区已满(@ref SC030IOT_I2C_BURST_MAX)
 *   时,先冲刷已累积的突发,再开始新的一段。
 * - @c next_reg 以 @c uint16_t 记录,避免 @c 0xff+1 回绕导致 @c 0x00 被误并。
 *
 * @param sensor          sensor 实例,内部使用其 @c slv_addr 作为 I2C 从地址。
 * @param regs            寄存器表,每个表项为 @c {寄存器地址, 寄存器值}。
 * @param regs_entry_len  寄存器表项数量。
 *
 * @return 0 表示成功;非 0 为底层 I2C 写入返回的错误码;
 *         @c sensor 或 @c regs 为 NULL 时返回 -1。
 */
static int set_regs(sensor_t *sensor, const uint8_t (*regs)[2], uint32_t regs_entry_len)
{
    /* buf: 突发写缓冲。buf[0] 存放本段起始寄存器地址,其后为连续地址的数据字节,
     *      故一次突发最多可写 (SC030IOT_I2C_BURST_MAX - 1) 个寄存器。
     * buf_len: buf 中已填充的字节数(含地址字节);为 0 表示当前没有待冲刷的突发。
     * next_reg: 当前突发段期望的下一个连续寄存器地址,用 16 位保存以便 0xff+1
     *           不回绕成 0x00,避免把新一页的 0x00 误判为与 0xff 连续而并入。
     */
    uint8_t buf[SC030IOT_I2C_BURST_MAX];
    uint32_t buf_len = 0;
    uint16_t next_reg = 0;
    int res = 0;

    if (!sensor || !regs) {
        return -1;
    }

    for (uint32_t i = 0; i < regs_entry_len; i++) {
        uint8_t reg = regs[i][0];
        uint8_t val = regs[i][1];

        /* 0xf0 是分页寄存器,不是普通数据寄存器:它切换后续 8 位地址所属的页。
         * 必须与数据写彻底隔离,否则一旦某个页值恰好等于 next_reg,就会被误并进
         * 上一段突发,导致写错寄存器或跨页污染。这里的处理是:
         *   1) 先把当前已累积的突发冲刷出去(并清空缓冲);
         *   2) 单独用一次 2 字节事务写页切换;
         *   3) 切页后延时 3ms 让分页生效,再继续处理下一条目(即该页的连续数据写)。
         * 注意:continue 后 buf_len 仍为 0,下一条数据寄存器会重新开启新的一段突发,
         * next_reg 也随之从新页的第一个真实寄存器地址重新计算。
         */
        if (reg == 0xf0) {
            if (buf_len > 0) {
                res = CAMERA_WRITE_RAW8(sensor->slv_addr, buf, (int)buf_len);
                if (res) {
                    return res;
                }
                buf_len = 0;
            }

            uint8_t pg[2] = {0xf0, val};
            res = CAMERA_WRITE_RAW8(sensor->slv_addr, pg, 2);
            if (res) {
                return res;
            }
            CAMERA_DELAY_MS(3);   // 切页后延时,等待分页切换生效再写该页数据
            continue;
        }

        /* 情况一:缓冲为空,当前寄存器作为新一段突发的起点。
         * buf[0] 放地址,buf[1] 放第一个数据,期望的下一个连续地址为 reg+1。
         */
        if (buf_len == 0) {
            buf[0] = reg;
            buf[1] = val;
            buf_len = 2;
            next_reg = (uint16_t)reg + 1U;
            continue;
        }

        /* 情况二:地址与上一项连续,且缓冲未满,追加到当前突发。
         * 依赖 sensor 的写地址自增特性:一次事务 [reg, v0, v1, ...] 会依次写入
         * reg、reg+1、reg+2 ...,从而把厂方期望原子更新的连续寄存器块合并写入。
         * buf_len < sizeof(buf) 保证 buf[buf_len] 不越界(缓冲满则落入下方冲刷分支)。
         */
        if ((buf_len < sizeof(buf)) && ((uint16_t)reg == next_reg)) {
            buf[buf_len++] = val;
            next_reg = (uint16_t)reg + 1U;
            continue;
        }

        /* 情况三:地址不连续或缓冲已满,先把已累积的突发冲刷出去,
         * 再以当前寄存器为起点开启新的一段。冲刷后延时 1ms,给上一段写入留出余量。
         */
        res = CAMERA_WRITE_RAW8(sensor->slv_addr, buf, (int)buf_len);
        if (res) {
            return res;
        }

        buf[0] = reg;
        buf[1] = val;
        buf_len = 2;
        next_reg = (uint16_t)reg + 1U;
        CAMERA_DELAY_MS(2);
    }

    /* 遍历结束后冲刷最后一段尚未写出的突发(regs_entry_len 为 0 时 buf_len 仍为 0,
     * 直接返回初始值 0)。
     */
    if (buf_len > 0) {
        res = CAMERA_WRITE_RAW8(sensor->slv_addr, buf, (int)buf_len);
    }

    return res;
}

static int set_reg_bits(sensor_t *sensor, int reg, uint8_t offset, uint8_t length, uint8_t value)
{
    int ret = 0;
    ret = get_reg(sensor, reg, 0xff);
    if(ret < 0){
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = set_reg(sensor, reg & 0xFFFF, 0xFFFF, value);
    return ret;
}

#define WRITE_REGS_OR_RETURN(regs, regs_entry_len) ret = set_regs(sensor, regs, regs_entry_len); if(ret){return ret;}
#define WRITE_REG_OR_RETURN(reg, val) ret = set_reg(sensor, reg, 0xFF, val); if(ret){return ret;}
#define SET_REG_BITS_OR_RETURN(reg, offset, length, val) ret = set_reg_bits(sensor, reg, offset, length, val); if(ret){return ret;}

static int set_hmirror(sensor_t *sensor, int enable)
{
    int ret = 0;
    if(enable) {
        SET_REG_BITS_OR_RETURN(0x3221, 1, 2, 0x3); // mirror on
    } else {
        SET_REG_BITS_OR_RETURN(0x3221, 1, 2, 0x0); // mirror off
    }

    return ret;
}

static int set_vflip(sensor_t *sensor, int enable)
{
    int ret = 0;
    if(enable) {
        SET_REG_BITS_OR_RETURN(0x3221, 5, 2, 0x3); // flip on
    } else {
        SET_REG_BITS_OR_RETURN(0x3221, 5, 2, 0x0); // flip off
    }

    return ret;
}

static int set_colorbar(sensor_t *sensor, int enable)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x0100, 7, 1, enable & 0xff); // enable test pattern mode

    return ret;
}

static int set_sharpness(sensor_t *sensor, int level)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x00e0, 1, 1, 1); // enable edge enhancement
    WRITE_REG_OR_RETURN(0x00d0, level & 0xFF); // base value
    WRITE_REG_OR_RETURN(0x00d2, (level >> 8) & 0xFF); // limit

    return ret;
}

static int set_agc_gain(sensor_t *sensor, int gain)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x0070, 1, 1, 1); // enable auto agc control
    WRITE_REG_OR_RETURN(0x0068, gain & 0xFF); // Window weight setting1
    WRITE_REG_OR_RETURN(0x0069, (gain >> 8) & 0xFF); // Window weight setting2
    WRITE_REG_OR_RETURN(0x006a, (gain >> 16) & 0xFF); // Window weight setting3
    WRITE_REG_OR_RETURN(0x006b, (gain >> 24) & 0xFF); // Window weight setting4

    return ret;
}

static int set_aec_value(sensor_t *sensor, int value)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x0070, 0, 1, 1); // enable auto aec control
    WRITE_REG_OR_RETURN(0x0072, value & 0xFF); // AE target

    return ret;
}

static int set_awb_gain(sensor_t *sensor, int value)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x00b0, 0, 1, 1); // enable awb control
    WRITE_REG_OR_RETURN(0x00c8, value & 0xFF); // blue gain
    WRITE_REG_OR_RETURN(0x00c9, (value>>8) & 0XFF); // red gain
    return ret;
}

static int set_saturation(sensor_t *sensor, int level)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x00f5, 5, 1, 0); // enable saturation control
    WRITE_REG_OR_RETURN(0x0149, level & 0xFF); // blue saturation gain (/128)
    WRITE_REG_OR_RETURN(0x014a, (level>>8) & 0XFF); // red saturation gain (/128)
    return ret;
}

static int set_contrast(sensor_t *sensor, int level)
{
    int ret = 0;
    SET_REG_BITS_OR_RETURN(0x00f5, 6, 1, 0); // enable contrast control
    WRITE_REG_OR_RETURN(0x014b, level); // contrast coefficient(/64)
    return ret;
}

static int reset(sensor_t *sensor)
{
    int ret = set_regs(sensor, sc030iot_default_init_regs, sizeof(sc030iot_default_init_regs)/(sizeof(uint8_t) * 2));

    // Delay
    CAMERA_DELAY_MS(50);

    // CAMERA_LOGI(TAG"set_reg=%0x", set_reg(sensor, 0x0100, 0xffff, 0x00)); // write 0x80 to enter test mode if you want to test the sensor
    // CAMERA_LOGI(TAG"0x0100=%0x", get_reg(sensor, 0x0100, 0xffff));
    if (ret) {
        CAMERA_LOGE(TAG"reset fail");
    }
    return ret;
}

static int start(sensor_t *sensor)
{
    // int ret = set_reg(sensor, 0x3100, 0xFF, 0x01);
    // CAMERA_DELAY_MS(10);
    // CAMERA_LOGI(TAG"start ret=%d, 0x3100=0x%02x, 0x0100=0x%02x, 0x017a=0x%02x, 0x0177=0x%02x",
    //             ret,
    //             get_reg(sensor, 0x3100, 0xFF),
    //             get_reg(sensor, 0x0100, 0xFF),
    //             get_reg(sensor, 0x017a, 0xFF),
    //             get_reg(sensor, 0x0177, 0xFF));
    // return ret;
    return 0;
}

static int stop(sensor_t *sensor)
{
    // int ret = set_reg(sensor, 0x3100, 0xFF, 0x00);
    // CAMERA_LOGI(TAG"stop ret=%d, 0x3100=0x%02x", ret, get_reg(sensor, 0x3100, 0xFF));
    // return ret;
    return 0;
}


#if 0
static int set_window(sensor_t *sensor, int offset_x, int offset_y, int w, int h)
{
    int ret = 0;
    //sc:H_start={0x0172[1:0],0x0170},H_end={0x0172[5:4],0x0171},
    WRITE_REG_OR_RETURN(0x0170, offset_x & 0xff);
    WRITE_REG_OR_RETURN(0x0171, (offset_x+w) & 0xff);
    WRITE_REG_OR_RETURN(0x0172, ((offset_x>>8) & 0x03) | (((offset_x+w)>>4)&0x30));

    //sc:V_start={0x0175[1:0],0x0173},H_end={0x0175[5:4],0x0174},
    WRITE_REG_OR_RETURN(0x0173, offset_y & 0xff);
    WRITE_REG_OR_RETURN(0x0174, (offset_y+h) & 0xff);
    WRITE_REG_OR_RETURN(0x0175, ((offset_y>>8) & 0x03) | (((offset_y+h)>>4)&0x30));

    CAMERA_DELAY_MS(10);

    return ret;
}

static int set_framesize(sensor_t *sensor, framesize_t framesize)
{
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    if(w>SC030_MAX_FRAME_WIDTH || h > SC030_MAX_FRAME_HIGH) {
        goto err;
    }

    uint16_t offset_x = (640-w) /2;
    uint16_t offset_y = (480-h) /2;

    if(set_window(sensor, offset_x, offset_y, w, h)) {
        goto err;
    }

    sensor->status.framesize = framesize;
    return 0;
err:
    CAMERA_LOGE(TAG"frame size err");
    return -1;
}
#endif


static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    int ret=0;

    switch (pixformat) {
    case PIXFORMAT_RGB565:
    case PIXFORMAT_RAW:
    case PIXFORMAT_GRAYSCALE:
        CAMERA_LOGE(TAG"Not support");
        return -1;
    case PIXFORMAT_YUV422: // For now, sc030/sc031 sensor only support YUV422.
        break;
    default:
        return -1;
    }

    sensor->pixformat = pixformat;

    return ret;
}

static pixformat_t get_pixformat(sensor_t *sensor)
{
    (void)sensor;
    return PIXFORMAT_YUV422;
}

static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    (void)sensor;

    if (!w || !h) {
        return -1;
    }

    *w = SC030_MAX_FRAME_WIDTH;
    *h = SC030_MAX_FRAME_HIGH;
    return 0;
}

static int init_status(sensor_t *sensor)
{
    sensor->pixformat = PIXFORMAT_YUV422;
    sensor->status.framesize = FRAMESIZE_VGA;
    return 0;
}

static int set_dummy(sensor_t *sensor, int val){ return -1; }

static int set_xclk(sensor_t *sensor, int timer, int xclk)
{
    int ret = 0;
    sensor->xclk_freq_hz = xclk * 1000000U;
    //ret = xclk_timer_conf(timer, sensor->xclk_freq_hz);
    return ret;
}

int sc030iot_detect(int slv_addr, sensor_id_t *id)
{
    if (SC030IOT_SCCB_ADDR == slv_addr) {
        uint8_t MIDL = CAMERA_READ_REG8(slv_addr, SC030_SENSOR_ID_LOW_REG);
        // uint8_t MIDH = CAMERA_READ_REG8(slv_addr, SC030_SENSOR_ID_HIGH_REG);
        // uint16_t PID = MIDH << 8 | MIDL;
        uint16_t PID = MIDL;
        if (SC030IOT_PID == PID) { /* SC030IOT新手册中要求只判断低字节 */
            id->PID = PID;
            return PID;
        } else {
            CAMERA_LOGI(TAG"Mismatch PID=0x%x", PID);
        }
    }
    return 0;
}

int sc030iot_init(sensor_t *sensor)
{
    // Set function pointers
    sensor->reset = reset;
    sensor->start = start;
    sensor->stop = stop;
    sensor->init_status = init_status;
    sensor->set_pixformat = set_pixformat;
    sensor->get_pixformat = get_pixformat;
    sensor->get_window = get_window;
    // sensor->set_framesize = set_framesize;

    sensor->set_saturation= set_saturation;
    sensor->set_colorbar = set_colorbar;
    sensor->set_hmirror = set_hmirror;
    sensor->set_vflip = set_vflip;
    sensor->set_sharpness = set_sharpness;
    sensor->set_agc_gain = set_agc_gain;
    sensor->set_aec_value = set_aec_value;
    sensor->set_awb_gain = set_awb_gain;
    sensor->set_contrast = set_contrast;
    //not supported
    sensor->set_denoise = set_dummy;
    sensor->set_quality = set_dummy;
    sensor->set_special_effect = set_dummy;
    sensor->set_wb_mode = set_dummy;
    sensor->set_ae_level = set_dummy;


    sensor->get_reg = get_reg;
    sensor->set_reg = set_reg;
    sensor->set_xclk = set_xclk;

    CAMERA_LOGD(TAG"sc030iot Attached");

    return 0;
}
