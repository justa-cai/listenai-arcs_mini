#ifndef __CAMERA_XFER_H__
#define __CAMERA_XFER_H__
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct pin_info {
    uint8_t pad;
    uint8_t pin;
    uint32_t func;
} pin_info_t;

/**
 * @brief SPI硬件配置结构体
 */
typedef struct {
    void     *spi_dev;          // SPI端口号
    uint32_t clock_freq_hz;     // 时钟频率
    uint8_t data_bits;          // 数据位数
    uint8_t mode;               // SPI模式 (CPOL/CPHA)
    uint8_t bit_order;          // 位序 (MSB/LSB)
    uint8_t role;               // 主从模式
    uint8_t dma_channel;        // DMA通道
    uint8_t use_hw_cs;          // 是否使用硬件片选
    
    // 引脚配置
    struct {
        pin_info_t clk;         // 引脚组
        pin_info_t mosi;        // 时钟引脚
        pin_info_t miso;        // MOSI引脚
        pin_info_t cs;          // MISO引脚
    } pins;
} spi_config_t;

/**
 * @brief DVP硬件配置结构体
 */
typedef struct {
    void     *dvp_dev;          // DVP设备
    uint16_t pixel_offset;      // 像素偏移
    uint16_t line_offset;       // 行偏移
    uint8_t input_format;       // 输入格式
    uint8_t pck_polarity;       // 像素时钟极性
    uint8_t vs_polarity;        // 垂直同步极性
    uint8_t hs_polarity;        // 水平同步极性
    uint8_t data_align;         // 数据对齐方式
    uint8_t dma_channel;        // DMA通道
    
    // 引脚配置
    struct {
        pin_info_t hsync;        // HSYNC引脚
        pin_info_t vsync;        // VSYNC引脚
        pin_info_t pclk;         // PCLK引脚
        pin_info_t data[8];      // DATA引脚
    } pins;
} dvp_config_t;

/**
 * @brief Camera数据传输接口配置结构体 (仅包含数据传输相关接口)
 */
typedef struct {
    union {
        spi_config_t spi_config;    // SPI数据传输配置
        dvp_config_t dvp_config;    // DVP数据传输配置
    };
} xfer_hw_config_t;


#if 1
// #define SPI_XFER_MISO_PIN           6        //D1
// #define SPI_XFER_WP_PIN             10       //D2
// #define SPI_XFER_HOLD_PIN           11       //D3

// #define SPI_XFER_DEBUG_PIN          0
#else
#define SPI_XFER_PIN_FUNC           CSK_IOMUX_FUNC_ALTER29
#define SPI_XFER_CS_PIN_FUNC        CSK_IOMUX_FUNC_DEFAULT

// #define SPI_XFER_PIN_PAD            CSK_IOMUX_PAD_A
// #define SPI_XFER_CLK_PAD            CSK_IOMUX_PAD_B
// #define SPI_XFER_CLK_PIN            7
// #define SPI_XFER_MOSI_PIN           7      //D0
// #define SPI_XFER_MISO_PIN           6      //D1
// #define SPI_XFER_WP_PIN             9      //D2
// #define SPI_XFER_HOLD_PIN           8      //D3
// #define SPI_XFER_CS_PIN             0

#define SPI_XFER_PIN_PAD            CSK_IOMUX_PAD_A
#define SPI_XFER_CLK_PAD            CSK_IOMUX_PAD_A
#define SPI_XFER_CLK_PIN            22
#define SPI_XFER_MOSI_PIN           25      //D0
#define SPI_XFER_MISO_PIN           24      //D1
// #define SPI_XFER_WP_PIN             21      //D2
// #define SPI_XFER_HOLD_PIN           20      //D3
#define SPI_XFER_CS_PIN             20

#define SPI_XFER_DEBUG_PIN          14

#endif

// #define CAMERA_XCLK_OUT_PAD         CSK_IOMUX_PAD_B
// #define CAMERA_XCLK_OUT_PIN         4




typedef void (*spi_recv_pp_cb)(uint8_t *buf, uint32_t len);

enum spi_xfer_mode {
    SPI_XFER_MODE_FRAME = 0,
    SPI_XFER_MODE_NOFRAME,
};

struct cam_ipeg_buf {
    unsigned int size;
    unsigned int readed;
    void *addr;
};

struct cam_ipeg_mem {
    unsigned char index;
    struct cam_ipeg_buf buf;
};

struct cam_xfer_queue {
    uint16_t width;
    uint16_t height;
    void *queue_in;
    void *queue_out;
};

struct cam_xfer_ops
{
    int (*cam_xfer_init)(const xfer_hw_config_t *config, struct cam_xfer_queue *queue);
    int (*cam_xfer_start)(void);
    int (*cam_xfer_stop)(void);
    int (*cam_xfer_deinit)(void);
    int (*cam_xfer_abort)(void);
    int (*cam_xfer_resume)(void);
    int (*cam_xfer_recv)(uint8_t *rxbuf, uint32_t len);
    int (*cam_xfer_pp_recv)(uint8_t *ping_buf, uint8_t *pong_buf, uint32_t len, spi_recv_pp_cb cb);
};

struct cam_xfer {
    struct cam_xfer_ops *ops;
    struct cam_xfer_queue queue;
    void *priv;
};

struct cam_xfer *cam_xfer_init(const xfer_hw_config_t *config, struct cam_xfer_queue *queue);
int cam_xfer_deinit(struct cam_xfer *xfer);

#ifdef __cplusplus
}
#endif
#endif
