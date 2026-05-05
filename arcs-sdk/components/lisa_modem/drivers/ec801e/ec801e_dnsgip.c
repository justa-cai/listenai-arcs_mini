/**
 * @file ec801e_dnsgip.c
 * @brief EC801E DNS URC helpers
 */

#include "drivers/ec801e/ec801e_dnsgip.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <string.h>

bool ec801e_dnsgip_try_extract_ip(at_arg_value_t *args, size_t count,
                                  char *ip_addr, size_t size, bool *done)
{
    struct in_addr addr;

    if (done) {
        *done = false;
    }

    if (!args || count < 2 ||
        args[0].type != AT_ARG_TYPE_STRING || !args[0].data.string_val.value ||
        strcmp(args[0].data.string_val.value, "dnsgip") != 0) {
        return false;
    }

    if (args[1].type == AT_ARG_TYPE_INT) {
        if (args[1].data.int_val != 0 && done) {
            *done = true;
        }
        return false;
    }

    if (!ip_addr || size < 16 ||
        args[1].type != AT_ARG_TYPE_STRING || !args[1].data.string_val.value ||
        inet_pton(AF_INET, args[1].data.string_val.value, &addr) != 1) {
        return false;
    }

    strncpy(ip_addr, args[1].data.string_val.value, size - 1);
    ip_addr[size - 1] = '\0';
    if (done) {
        *done = true;
    }
    return true;
}
