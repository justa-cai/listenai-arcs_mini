#ifndef FLASH_MSG_H
#define FLASH_MSG_H

#define CMD_FLASH_INIT    (0)
#define CMD_FLASH_WRITE   (1)

int flash_msg_init(void);
int flash_msg_send(uint8_t cmd);
int flash_set_flash_flag(void);
#endif