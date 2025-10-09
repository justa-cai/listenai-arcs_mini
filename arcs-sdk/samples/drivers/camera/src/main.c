#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "Driver_SPI.h"
#include "Driver_I2C.h"
#include "Driver_UART.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "lisa_log.h"
#include "camera_xfer.h"
#include "camera_core.h"

volatile uint32_t uart_event = 0;
uint8_t uart_buf[614400]  __attribute__((section(".psram.data"))) = {0};
static void UART_EventCallback(uint32_t event, void *workspace)
{
	uart_event |= event;
}

static void *UART_Handler = NULL;

#define UART1_IO_TX_PAD (CSK_IOMUX_PAD_A)
#define UART1_IO_TX_PIN (21)
#define UART1_IO_TX_SEL (CSK_IOMUX_FUNC_ALTER3)

void serail_init(void)
{
	__HAL_CRM_UART1_CLK_ENABLE();

	IOMuxManager_PinConfigure(UART1_IO_TX_PAD, UART1_IO_TX_PIN, UART1_IO_TX_SEL);
	// IOMuxManager_PinConfigure(UART1_IO_RX_PAD, UART1_IO_RX_PIN, UART1_IO_RX_SEL);

	UART_Handler = UART1();
	UART_Initialize(UART_Handler, UART_EventCallback, NULL);

	UART_PowerControl(UART_Handler, CSK_POWER_FULL);

	UART_Control(UART_Handler,
                CSK_UART_MODE_ASYNCHRONOUS_TIMEOUT | CSK_UART_DATA_BITS_8 |
			     CSK_UART_PARITY_NONE | CSK_UART_STOP_BITS_1 |
			     CSK_UART_FLOW_CONTROL_NONE | CSK_UART_Function_CONTROL_Dma |
			     CSK_UART_GPIO_CONTROL_DEFAULT,
		    3000000);

	UART_Control(UART_Handler, CSK_UART_CONTROL_TX, 1);
	// UART_Control(UART_Handler, CSK_UART_CONTROL_RX, 1);

	uart_event = 0;
}

void serial_send(const uint8_t *s, uint32_t len)
{
	uart_event = 0;

	LOGI("%x %x %x %x %x %x\r\n", s[0], s[1], s[2], s[len - 3], s[len - 2], s[len - 1]);
	int err = UART_Send(UART_Handler, s, len);
	if (err) {
		printf("uart send failed, err: %d\n", err);
		return;
	}
}


int main(int argc, char **argv)
{
    camera_config_t camera_cfg = {
        .xclk_freq_hz = 24000000,
        .pixel_format = PIXFORMAT_YUV422,
        .frame_size   = FRAMESIZE_VGA,
        .buf_count    = 3,
        .colorbar     = 0,
        .i2c_config = {
            .i2c_dev = I2C0(),
            .pins = {
                .sda = {
                    .pad = CSK_IOMUX_PAD_A,
                    .pin = 22,
                    .func = CSK_IOMUX_FUNC_ALTER8
                },
                .scl = {
                    .pad = CSK_IOMUX_PAD_A,
                    .pin = 23,
                    .func = CSK_IOMUX_FUNC_ALTER8
                }
            }
        },
        .xfer_config = {
            .dvp_config = {
                .dvp_dev = DVP0(),
                .input_format = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
                .pck_polarity = DVP_POL_FALLING,
                .vs_polarity  = DVP_POL_FALLING,
                .hs_polarity  = DVP_POL_RISING,
                .data_align   = DVP_DATA_ALIGN_LEFT,
                .dma_channel  = gp_dma_ch3,
                .pins = {
                    .hsync = {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 10,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    .vsync = {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 11,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    .pclk = {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 12,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    .data = {
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 13,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 14,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 15,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 16,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 17,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 18,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 19,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        },
                        {
                            .pad = CSK_IOMUX_PAD_A,
                            .pin = 20,
                            .func = CSK_IOMUX_FUNC_ALTER16
                        }
                    }
                }
            },
        }
    };

    serail_init();

    camera_init(&camera_cfg);

    camera_start();

    uart_event = CSK_UART_EVENT_SEND_COMPLETE;
    int send_cnt = 0;
    while (1) {
        struct cam_ipeg_mem *spi_mem = NULL;

        spi_mem = camera_dqbuf(spi_mem, 1000);
        if (spi_mem) {
            if ((uart_event & CSK_UART_EVENT_SEND_COMPLETE) && (send_cnt++ < 20)) {
                memcpy(uart_buf,spi_mem->buf.addr, sizeof(uart_buf));
                uart_event = 0;
                serial_send(uart_buf, spi_mem->buf.size);
            }
            camera_qbuf(spi_mem);
        }
        else {
            LOGE("camera_dqbuf timeout");
        }
    }

    camera_stop();
}
