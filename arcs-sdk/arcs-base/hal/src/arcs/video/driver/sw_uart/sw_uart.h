#ifndef __SW_UART_H
#define __SW_UART_H

#ifdef __cplusplus
 extern "C" {
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


int sw_uart_init(void);
void sw_uart_send_byte(const uint8_t data);
int sw_printf(const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif /* __CAMERA_I2C_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
