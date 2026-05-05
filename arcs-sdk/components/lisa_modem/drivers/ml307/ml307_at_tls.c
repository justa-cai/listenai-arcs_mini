/**
 * @file ml307_at_tls.c
 * @brief Reserved ML307 AT TLS helpers
 */

#include "drivers/ml307/ml307_at_tls.h"
#include <stdio.h>

bool ml307_at_tls_configure_socket(at_client_t *client, int connect_id, bool enable)
{
    char command[64];

    if (!client || connect_id <= 0) {
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPCFG=\"ssl\",%d,%d,0", connect_id, enable ? 1 : 0);
    return at_client_send_cmd(client, command, 1000, true);
}
