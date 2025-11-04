#include "lisaui_type.h"
#include "lisaui_sys_events.h"
#include "ebus/ebus.h"

lisaui_err_t lisaui_sys_events_init(void){
    ebus_handle_t *bus;
    ebus_chn_t *chn;

    // Create bus
    bus = ebus_create(LISAUI_BUS_NAME);
    if (bus == NULL) {
        return -1;
    }
    
    // Create wordbook channel
    chn = ebus_chn_create_attach(bus, LISAUI_SYSTEM_NOTIFY_CHANNEL_NAME);
    if (chn == NULL) {
        return -1;
    }
    return LISAUI_ERR_OK;
}