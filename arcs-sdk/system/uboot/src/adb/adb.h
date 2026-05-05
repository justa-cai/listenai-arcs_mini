#ifndef __ADB_H__
#define __ADB_H__

#include <stdint.h>
#include "usb_config.h"
#include "usb_util.h"

#define ADB_TX_PAYLOAD	    (64 * 1024)
#define ADB_RX_PAYLOAD	    (64 * 1024)
#define ADB_MSG_SIZE        (24)
#define ADB_VERSION         0x01000000
#define ADB_CNXN_FEAT		"device::" \
							"ro.product.name=boot;" \
							"ro.product.model=listenai;" \
							"ro.product.device=recovery-mode;" \
							"features=cmd,shell_v1"

#define ID4C(a,b,c,d)	    ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define ADB_MAIN_ID_CNXN    ID4C('C', 'N', 'X', 'N')
#define ADB_MAIN_ID_OPEN    ID4C('O', 'P', 'E', 'N')
#define ADB_MAIN_ID_OKAY    ID4C('O', 'K', 'A', 'Y')
#define ADB_MAIN_ID_CLSE    ID4C('C', 'L', 'S', 'E')
#define ADB_MAIN_ID_WRTE    ID4C('W', 'R', 'T', 'E')
#define ADB_SYNC_ID_LIST    ID4C('L', 'I', 'S', 'T')
#define ADB_SYNC_ID_RECV    ID4C('R', 'E', 'C', 'V')
#define ADB_SYNC_ID_SEND    ID4C('S', 'E', 'N', 'D')
#define ADB_SYNC_ID_STAT    ID4C('S', 'T', 'A', 'T')
#define ADB_SYNC_ID_FAIL    ID4C('F', 'A', 'I', 'L')
#define ADB_SYNC_ID_DATA    ID4C('D', 'A', 'T', 'A')
#define ADB_SYNC_ID_DONE    ID4C('D', 'O', 'N', 'E')
#define ADB_SYNC_ID_OKAY    ID4C('O', 'K', 'A', 'Y')
#define ADB_SYNC_ID_QUIT    ID4C('Q', 'U', 'I', 'T')
#define ADB_SYNC_ID_DENT    ID4C('D', 'E', 'N', 'T')

typedef struct {
    USB_MEM_ALIGNX struct {
		uint32_t mcmd;		/* command identifier(CNXN/OPEN/WRTE/CLSE/OKAY/...)	*/
		uint32_t arg0;      /* 1st argument                            			*/
		uint32_t arg1;      /* 2nd argument                           			*/
		uint32_t datlen;	/* length of payload (0 is allowed)          		*/
		uint32_t datsum;	/* sum of data payload                     			*/
		uint32_t magic;     /* command ^ 0xffffffff                      		*/
	};
    USB_MEM_ALIGNX uint8_t data[0];
} adb_packet_t;

struct adb_split {
	uint8_t *spilt;
	uint32_t len;
};

void adb_init(void);
void adb_close(uint32_t local_id, uint32_t remote_id);
void adb_write(uint32_t local_id, uint32_t remote_id, uint8_t *data, uint32_t len);
adb_packet_t *adb_packet_get();
void adb_packet_free(adb_packet_t *p);

#endif
