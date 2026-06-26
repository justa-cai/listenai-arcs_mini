#ifndef __MODEL_MODEM_H__
#define __MODEL_MODEM_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool is_modem_mode;
    bool active;
    bool connected;
    bool switching;
    int signal_rssi;
    int signal_ber;
    uint8_t signal_level;
} model_modem_info_t;

int model_modem_init(void);
int model_modem_deinit(void);
int model_modem_poll(void);
int model_modem_get_info(model_modem_info_t *info);

#endif
