#ifndef __ADB_H__
#define __ADB_H__

#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257

#define A_VERSION   0x01000000

#ifdef CONFIG_ADB_MAX_PAYLOAD_SIZE
#define MAX_PAYLOAD CONFIG_ADB_MAX_PAYLOAD_SIZE
#else
#define MAX_PAYLOAD 4096
#endif

#include <stdint.h>

struct message {
	uint32_t command;     /* command identifier constant      */
	uint32_t arg0;        /* first argument                   */
	uint32_t arg1;        /* second argument                  */
	uint32_t data_length; /* length of payload (0 is allowed) */
	uint32_t data_check;  /* checksum of data payload         */
	uint32_t magic;       /* command ^ 0xffffffff             */
};

struct adb_split {
	uint8_t *spilt;
	uint32_t len;
};
typedef struct {
    struct message msg;
    uint8_t data[0];
} adb_packet_t;

void adb_init(void);
void adb_close(uint32_t local_id, uint32_t remote_id);
void adb_write(uint32_t local_id, uint32_t remote_id, uint8_t *data, uint32_t len);
void adb_packet_free(adb_packet_t *p);

#endif
